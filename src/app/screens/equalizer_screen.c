#include "screens/equalizer_screen.h"

#include <stddef.h>

#include "hal/audio.h"
#include "ui/header.h"
#include "ui/list_navigation.h"

static const char CHECK_MARK[] = "\xE2\x9C\x93";
static const char *const CONFIRM_OPTIONS[2] = {"Cancel", "Confirm"};

static void apply_eq(const Settings *settings)
{
    hal_audio_set_eq(settings->eq_enabled, settings->eq_gains);
}

static int main_item_count(const void *context)
{
    const EqualizerScreen *screen = context;
    const Settings *settings = &screen->store->settings;
    if (settings->eq_preset == EQ_PRESET_CUSTOM &&
        settings->eq_user_preset_count < EQ_USER_PRESET_CAPACITY) {
        return EQ_ROW_SAVE_PRESET + 1;
    }
    return EQ_DEFAULT_ROW_COUNT;
}

static const char *main_item_label(const void *context, int index)
{
    (void)context;
    switch (index) {
    case EQ_ROW_ENABLED:
        return "Equalizer";
    case EQ_ROW_PRESET:
        return "Preset";
    case EQ_ROW_CUSTOM_EQ:
        return "Custom EQ";
    case EQ_ROW_RESET:
        return "Reset Equalizer";
    case EQ_ROW_SAVE_PRESET:
        return "Save Preset";
    default:
        return "";
    }
}

static const char *main_item_detail(const void *context, int index)
{
    const EqualizerScreen *screen = context;
    const Settings *settings = &screen->store->settings;
    switch (index) {
    case EQ_ROW_ENABLED:
        return settings->eq_enabled ? "On" : "Off";
    case EQ_ROW_PRESET:
        return settings_preset_name(settings, settings->eq_preset);
    default:
        return "";
    }
}

static bool main_item_opens(const void *context, int index)
{
    (void)context;
    return index != EQ_ROW_ENABLED;
}

static int preset_item_count(const void *context)
{
    const EqualizerScreen *screen = context;
    return EQ_PRESET_COUNT + screen->store->settings.eq_user_preset_count;
}

static const char *preset_item_label(const void *context, int index)
{
    const EqualizerScreen *screen = context;
    const Settings *settings = &screen->store->settings;
    if (index < EQ_PRESET_COUNT) {
        return eq_preset(index)->name;
    }
    int user_idx = index - EQ_PRESET_COUNT;
    if (user_idx >= 0 && user_idx < settings->eq_user_preset_count) {
        return settings->eq_user_presets[user_idx].name;
    }
    return "";
}

static const char *preset_item_detail(const void *context, int index)
{
    const EqualizerScreen *screen = context;
    const Settings *settings = &screen->store->settings;
    if (index < EQ_PRESET_COUNT) {
        return settings->eq_preset == index ? CHECK_MARK : NULL;
    }
    int user_idx = index - EQ_PRESET_COUNT;
    if (user_idx >= 0 && user_idx < settings->eq_user_preset_count) {
        return settings->eq_preset == settings->eq_user_presets[user_idx].id ? CHECK_MARK : NULL;
    }
    return NULL;
}

static int confirm_current(const void *context)
{
    (void)context;
    return 0;
}

static void choose_reset_option(void *context, int index)
{
    EqualizerScreen *screen = context;
    if (index == 1) {
        Settings *settings = &screen->store->settings;
        for (int band = 0; band < EQ_BAND_COUNT; band++) {
            settings->eq_gains[band] = 0;
        }
        settings->eq_preset = 0;
        apply_eq(settings);
        settings_store_save(screen->store);
    }
}

static ScreenAction open_reset_dialog(EqualizerScreen *screen)
{
    ChoiceConfig config = {
        .title = "Reset Equalizer?",
        .options = CONFIRM_OPTIONS,
        .option_count = 2,
        .current = confirm_current,
        .choose = choose_reset_option,
        .context = screen,
        .theme = screen->theme,
    };
    return screen_action_push(choice_screen_init(&screen->reset_choice, config));
}

static void choose_delete_option(void *context, int index)
{
    EqualizerScreen *screen = context;
    if (index == 1) {
        settings_delete_user_preset(&screen->store->settings, screen->pending_delete_id);
        settings_store_save(screen->store);
    }
}

static ScreenAction open_delete_dialog(EqualizerScreen *screen, uint8_t preset_id)
{
    screen->pending_delete_id = preset_id;
    ChoiceConfig config = {
        .title = "Delete Preset?",
        .options = CONFIRM_OPTIONS,
        .option_count = 2,
        .current = confirm_current,
        .choose = choose_delete_option,
        .context = screen,
        .theme = screen->theme,
    };
    return screen_action_push(choice_screen_init(&screen->delete_choice, config));
}

static ScreenAction select_preset_item(void *context, int index)
{
    EqualizerScreen *screen = context;
    Settings *settings = &screen->store->settings;
    if (index < EQ_PRESET_COUNT) {
        settings->eq_preset = (uint8_t)index;
        const EqPreset *preset = eq_preset(index);
        for (int band = 0; band < EQ_BAND_COUNT; band++) {
            settings->eq_gains[band] = preset->gains[band];
        }
    } else {
        int user_idx = index - EQ_PRESET_COUNT;
        if (user_idx >= 0 && user_idx < settings->eq_user_preset_count) {
            settings->eq_preset = settings->eq_user_presets[user_idx].id;
            for (int band = 0; band < EQ_BAND_COUNT; band++) {
                settings->eq_gains[band] = settings->eq_user_presets[user_idx].gains[band];
            }
        }
    }
    apply_eq(settings);
    settings_store_save(screen->store);
    return screen_action_pop();
}

static ScreenAction hold_preset_item(void *context, int index)
{
    EqualizerScreen *screen = context;
    Settings *settings = &screen->store->settings;
    if (index >= EQ_PRESET_COUNT) {
        int user_idx = index - EQ_PRESET_COUNT;
        if (user_idx >= 0 && user_idx < settings->eq_user_preset_count) {
            return open_delete_dialog(screen, settings->eq_user_presets[user_idx].id);
        }
    }
    return screen_action_none();
}

static ScreenAction preset_screen_handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    ListSelection selection = {
        .select = select_preset_item, .hold = hold_preset_item, .context = screen};
    return list_navigation_handle(&screen->preset_list, event, now_ms, selection);
}

static void preset_screen_enter(void *context, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
    const Settings *settings = &screen->store->settings;
    int selected = 0;
    if (settings->eq_preset < EQ_PRESET_COUNT) {
        selected = settings->eq_preset;
    } else {
        int user_idx = settings_find_user_preset_by_id(settings, settings->eq_preset);
        if (user_idx >= 0) {
            selected = EQ_PRESET_COUNT + user_idx;
        }
    }
    list_view_select(&screen->preset_list, selected, now_ms);
}

static void preset_screen_update(void *context, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
}

static void preset_screen_draw(void *context, Canvas *canvas)
{
    EqualizerScreen *screen = context;
    const Theme *theme = screen->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, screen->preset_screen.title);
    list_view_draw(&screen->preset_list, canvas, content, screen->now_ms);
}

static ScreenAction open_presets(EqualizerScreen *screen)
{
    screen->preset_model = (ListModel){
        .context = screen,
        .count = preset_item_count,
        .label = preset_item_label,
        .detail = preset_item_detail,
        .opens = NULL,
        .detail_max_width = 24,
        .section = NULL,
    };
    list_view_init(&screen->preset_list, &screen->preset_model, screen->theme, 0);
    screen->preset_screen = (Screen){
        .title = "Preset",
        .context = screen,
        .full_screen = false,
        .paints_every_pixel = false,
        .hold_volume_keys = false,
        .stays_on_wake = true,
        .enter = preset_screen_enter,
        .handle_key = preset_screen_handle_key,
        .update = preset_screen_update,
        .draw = preset_screen_draw,
    };
    return screen_action_push(&screen->preset_screen);
}

static ScreenAction select_main_item(void *context, int index)
{
    EqualizerScreen *screen = context;
    Settings *settings = &screen->store->settings;
    switch (index) {
    case EQ_ROW_ENABLED:
        settings->eq_enabled = !settings->eq_enabled;
        apply_eq(settings);
        settings_store_save(screen->store);
        return screen_action_none();
    case EQ_ROW_PRESET:
        return open_presets(screen);
    case EQ_ROW_CUSTOM_EQ:
        return screen_action_push(
            custom_eq_screen_init(&screen->custom_eq, screen->store, screen->theme));
    case EQ_ROW_RESET:
        return open_reset_dialog(screen);
    case EQ_ROW_SAVE_PRESET:
        return screen_action_push(
            save_preset_screen_init(&screen->save_preset, screen->store, screen->theme));
    default:
        return screen_action_none();
    }
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        settings_store_save(screen->store);
        return screen_action_pop();
    }
    ListSelection selection = {.select = select_main_item, .hold = NULL, .context = screen};
    return list_navigation_handle(&screen->list, event, now_ms, selection);
}

static void enter(void *context, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
    list_view_restart_marquee(&screen->list, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    EqualizerScreen *screen = context;
    const Theme *theme = screen->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, screen->screen.title);
    list_view_draw(&screen->list, canvas, content, screen->now_ms);
}

Screen *equalizer_screen_init(EqualizerScreen *eq_screen, SettingsStore *store,
                              const Theme *theme)
{
    eq_screen->store = store;
    eq_screen->theme = theme;
    eq_screen->now_ms = 0;
    eq_screen->pending_delete_id = 0;
    eq_screen->model = (ListModel){
        .context = eq_screen,
        .count = main_item_count,
        .label = main_item_label,
        .detail = main_item_detail,
        .opens = main_item_opens,
        .detail_max_width = 80,
        .section = NULL,
    };
    list_view_init(&eq_screen->list, &eq_screen->model, theme, 0);
    eq_screen->screen = (Screen){
        .title = "Equalizer",
        .context = eq_screen,
        .full_screen = false,
        .paints_every_pixel = false,
        .hold_volume_keys = false,
        .stays_on_wake = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &eq_screen->screen;
}
