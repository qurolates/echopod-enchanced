#include "screens/equalizer_screen.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/audio.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"

enum {
    ROW_ENABLED,
    ROW_PRESET,
    ROW_FLAT,
    ROW_BAND_100,
    ROW_BAND_300,
    ROW_BAND_1K,
    ROW_BAND_3K,
    ROW_BAND_10K,
    ROW_COUNT,
    PRESET_OPTION_COUNT = EQ_PRESET_COUNT + 1,
    PANEL_WIDTH = 84,
    PANEL_PAD = 4,
    BAR_WIDTH = 10,
    BAR_GAP = 5,
    BAR_MAX_HEIGHT = 44,
    LABEL_HEIGHT = 16
};

static const char *const PRESET_OPTIONS[PRESET_OPTION_COUNT] = {
    "Flat", "Acoustic", "Bass Boost", "Deep Bass", "Pop", "Rock",
    "Alternative Rock", "Metal", "Punk", "Hip-Hop", "Electronic / EDM",
    "R&B", "Jazz", "Classical", "Vocal", "Folk / Country", "Ambient", "Custom"};

static void apply_eq(const Settings *settings)
{
    hal_audio_set_eq(settings->eq_enabled, settings->eq_gains);
}

static int item_count(const void *context)
{
    (void)context;
    return ROW_COUNT;
}

static const char *item_label(const void *context, int index)
{
    (void)context;
    switch (index) {
    case ROW_ENABLED:
        return "Equalizer";
    case ROW_PRESET:
        return "Preset";
    case ROW_FLAT:
        return "Reset to Flat";
    default:
        return eq_band_name(index - ROW_BAND_100);
    }
}

static void format_gain(int8_t gain, bool active, char *buffer, size_t capacity)
{
    StringBuilder b = string_builder_make(buffer, capacity);
    if (active) {
        string_builder_append(&b, "< ");
    }
    if (gain > 0) {
        string_builder_append_char(&b, '+');
        string_builder_append_unsigned(&b, (uint32_t)gain);
    } else if (gain < 0) {
        string_builder_append_char(&b, '-');
        string_builder_append_unsigned(&b, (uint32_t)(-gain));
    } else {
        string_builder_append_char(&b, '0');
    }
    string_builder_append(&b, " dB");
    if (active) {
        string_builder_append(&b, " >");
    }
}

static void refresh_buffers(EqualizerScreen *screen)
{
    const Settings *settings = &screen->store->settings;
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        bool active = screen->adjusting && (screen->list.selected == band + ROW_BAND_100);
        format_gain(settings->eq_gains[band], active, screen->value_buffers[band],
                    sizeof screen->value_buffers[band]);
    }
}

static const char *item_detail(const void *context, int index)
{
    const EqualizerScreen *screen = context;
    const Settings *settings = &screen->store->settings;
    if (index == ROW_ENABLED) {
        return settings->eq_enabled ? "On" : "Off";
    }
    if (index == ROW_PRESET) {
        return eq_preset(settings->eq_preset)->name;
    }
    if (index == ROW_FLAT) {
        return NULL;
    }
    int band = index - ROW_BAND_100;
    return screen->value_buffers[band];
}

static bool item_opens(const void *context, int index)
{
    (void)context;
    return index == ROW_PRESET;
}

static int current_preset(const void *context)
{
    const EqualizerScreen *screen = context;
    return screen->store->settings.eq_preset;
}

static void choose_preset(void *context, int index)
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
        settings->eq_preset = EQ_PRESET_CUSTOM;
    }
    apply_eq(settings);
    refresh_buffers(screen);
}

static ScreenAction open_presets(EqualizerScreen *screen)
{
    ChoiceConfig config = {
        .title = "Preset",
        .options = PRESET_OPTIONS,
        .option_count = PRESET_OPTION_COUNT,
        .current = current_preset,
        .choose = choose_preset,
        .context = screen,
        .theme = screen->theme,
    };
    return screen_action_push(choice_screen_init(&screen->preset_choice, config));
}

static ScreenAction select_item(void *context, int index)
{
    EqualizerScreen *screen = context;
    Settings *settings = &screen->store->settings;
    if (index == ROW_ENABLED) {
        settings->eq_enabled = !settings->eq_enabled;
        apply_eq(settings);
        return screen_action_none();
    }
    if (index == ROW_PRESET) {
        return open_presets(screen);
    }
    if (index == ROW_FLAT) {
        settings->eq_preset = 0;
        for (int band = 0; band < EQ_BAND_COUNT; band++) {
            settings->eq_gains[band] = 0;
        }
        apply_eq(settings);
        refresh_buffers(screen);
        return screen_action_none();
    }
    screen->adjusting = true;
    refresh_buffers(screen);
    return screen_action_none();
}

static ScreenAction handle_adjusting_key(EqualizerScreen *screen, KeyEvent event)
{
    Settings *settings = &screen->store->settings;
    int band = screen->list.selected - ROW_BAND_100;
    bool is_step = event.gesture == KEY_GESTURE_PRESS || event.gesture == KEY_GESTURE_REPEAT;
    if (event.key == KEY_VOLUME_UP && is_step) {
        if (settings->eq_gains[band] < EQ_GAIN_MAX) {
            settings->eq_gains[band]++;
            settings->eq_preset = EQ_PRESET_CUSTOM;
            apply_eq(settings);
            refresh_buffers(screen);
        }
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_DOWN && is_step) {
        if (settings->eq_gains[band] > EQ_GAIN_MIN) {
            settings->eq_gains[band]--;
            settings->eq_preset = EQ_PRESET_CUSTOM;
            apply_eq(settings);
            refresh_buffers(screen);
        }
        return screen_action_none();
    }
    if ((event.key == KEY_PLAY || event.key == KEY_BACK) && event.gesture == KEY_GESTURE_PRESS) {
        screen->adjusting = false;
        refresh_buffers(screen);
        return screen_action_none();
    }
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
    if (screen->adjusting) {
        return handle_adjusting_key(screen, event);
    }
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        settings_store_save(screen->store);
        return screen_action_pop();
    }
    ListSelection selection = {.select = select_item, .hold = NULL, .context = screen};
    ScreenAction action = list_navigation_handle(&screen->list, event, now_ms, selection);
    refresh_buffers(screen);
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
    screen->adjusting = false;
    refresh_buffers(screen);
    list_view_restart_marquee(&screen->list, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    EqualizerScreen *screen = context;
    screen->now_ms = now_ms;
}

static Color bar_color(const EqualizerScreen *screen, int band)
{
    int selected_band = screen->list.selected - ROW_BAND_100;
    if (selected_band == band) {
        return screen->adjusting ? screen->theme->colors.selection_top : screen->theme->colors.text;
    }
    return screen->theme->colors.text_secondary;
}

static void draw_bar(const EqualizerScreen *screen, Canvas *canvas, int band, Rect area, int y_mid)
{
    int8_t gain = screen->store->settings.eq_gains[band];
    int height = (int8_t)(gain >= 0 ? gain : -gain) * BAR_MAX_HEIGHT / EQ_GAIN_MAX;
    int x = area.x + band * (BAR_WIDTH + BAR_GAP);
    Color color = bar_color(screen, band);
    if (gain > 0) {
        canvas_fill_rect(canvas, (Rect){x, y_mid - height, BAR_WIDTH, height}, color);
    } else if (gain < 0) {
        canvas_fill_rect(canvas, (Rect){x, y_mid, BAR_WIDTH, height}, color);
    } else {
        canvas_fill_rect(canvas, (Rect){x, y_mid - 1, BAR_WIDTH, 2}, color);
    }
}

static void draw_panel(const EqualizerScreen *screen, Canvas *canvas, Rect area)
{
    const Theme *theme = screen->theme;
    int y_mid = area.y + (area.height - LABEL_HEIGHT) / 2;
    canvas_fill_rect(canvas, (Rect){area.x, y_mid, area.width, 1}, theme->colors.divider);
    int total_width = EQ_BAND_COUNT * BAR_WIDTH + (EQ_BAND_COUNT - 1) * BAR_GAP;
    int start_x = area.x + (area.width - total_width) / 2;
    Rect bars_area = {start_x, area.y, total_width, area.height - LABEL_HEIGHT};
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        draw_bar(screen, canvas, band, bars_area, y_mid);
    }
    static const char *const SHORT_NAMES[EQ_BAND_COUNT] = {"100", "300", "1k", "3k", "10k"};
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    int label_y = area.y + area.height - LABEL_HEIGHT;
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        int x = start_x + band * (BAR_WIDTH + BAR_GAP) - 3;
        label_draw(canvas, style, (Label){.area = {x, label_y, BAR_WIDTH + 6, LABEL_HEIGHT},
                                          .text = SHORT_NAMES[band]});
    }
}

static void draw(void *context, Canvas *canvas)
{
    EqualizerScreen *screen = context;
    const Theme *theme = screen->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, screen->screen.title);
    Rect list_area = {content.x, content.y, content.width - PANEL_WIDTH, content.height};
    Rect side_area = {content.x + content.width - PANEL_WIDTH + PANEL_PAD, content.y,
                      PANEL_WIDTH - 2 * PANEL_PAD, content.height};
    list_view_draw(&screen->list, canvas, list_area, screen->now_ms);
    draw_panel(screen, canvas, side_area);
}

Screen *equalizer_screen_init(EqualizerScreen *eq_screen, SettingsStore *store,
                              const Theme *theme)
{
    eq_screen->store = store;
    eq_screen->theme = theme;
    eq_screen->adjusting = false;
    eq_screen->model = (ListModel){
        .context = eq_screen,
        .count = item_count,
        .label = item_label,
        .detail = item_detail,
        .opens = item_opens,
        .detail_max_width = 72,
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
