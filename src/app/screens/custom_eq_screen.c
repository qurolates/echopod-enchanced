#include "screens/custom_eq_screen.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/audio.h"
#include "ui/header.h"
#include "ui/label.h"

enum {
    Y_MID = 88,
    BAR_RANGE = 42,
    GRID_LEFT = 34,
    GRID_RIGHT = 310,
    TRACK_WIDTH = 2,
    FILL_WIDTH = 12,
    THUMB_WIDTH = 28,
    THUMB_HEIGHT = 6,
    LABEL_HEIGHT = 14
};

static const int BAND_CENTERS[EQ_BAND_COUNT] = {61, 116, 172, 227, 282};
static const char *const SHORT_NAMES[EQ_BAND_COUNT] = {"100", "300", "1k", "3k", "10k"};

static void apply_eq(const Settings *settings)
{
    hal_audio_set_eq(settings->eq_enabled, settings->eq_gains);
}

static void format_band_title(int band, int8_t gain, bool adjusting, char *buffer,
                              size_t capacity)
{
    StringBuilder b = string_builder_make(buffer, capacity);
    if (adjusting) {
        string_builder_append(&b, "< ");
    }
    string_builder_append(&b, eq_band_name(band));
    string_builder_append(&b, ": ");
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
    if (adjusting) {
        string_builder_append(&b, " >");
    }
}

static ScreenAction handle_adjusting_key(CustomEqScreen *screen, KeyEvent event)
{
    Settings *settings = &screen->store->settings;
    int band = screen->selected_band;
    bool is_step = event.gesture == KEY_GESTURE_PRESS || event.gesture == KEY_GESTURE_REPEAT;
    if (event.key == KEY_VOLUME_UP && is_step) {
        if (settings->eq_gains[band] < EQ_GAIN_MAX) {
            settings->eq_gains[band]++;
            settings->eq_preset = EQ_PRESET_CUSTOM;
            apply_eq(settings);
        }
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_DOWN && is_step) {
        if (settings->eq_gains[band] > EQ_GAIN_MIN) {
            settings->eq_gains[band]--;
            settings->eq_preset = EQ_PRESET_CUSTOM;
            apply_eq(settings);
        }
        return screen_action_none();
    }
    if ((event.key == KEY_PLAY || event.key == KEY_BACK) && event.gesture == KEY_GESTURE_PRESS) {
        screen->adjusting = false;
        return screen_action_none();
    }
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    CustomEqScreen *screen = context;
    screen->now_ms = now_ms;
    if (screen->adjusting) {
        return handle_adjusting_key(screen, event);
    }
    bool is_step = event.gesture == KEY_GESTURE_PRESS || event.gesture == KEY_GESTURE_REPEAT;
    if (event.key == KEY_VOLUME_DOWN && is_step) {
        if (screen->selected_band > 0) {
            screen->selected_band--;
        }
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_UP && is_step) {
        if (screen->selected_band < EQ_BAND_COUNT - 1) {
            screen->selected_band++;
        }
        return screen_action_none();
    }
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        screen->adjusting = true;
        return screen_action_none();
    }
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        settings_store_save(screen->store);
        return screen_action_pop();
    }
    return screen_action_none();
}

static void enter(void *context, uint32_t now_ms)
{
    CustomEqScreen *screen = context;
    screen->now_ms = now_ms;
    screen->adjusting = false;
}

static void update(void *context, uint32_t now_ms)
{
    CustomEqScreen *screen = context;
    screen->now_ms = now_ms;
}

static void draw_grid(Canvas *canvas, const Theme *theme)
{
    int grid_w = GRID_RIGHT - GRID_LEFT;
    canvas_fill_rect(canvas, (Rect){GRID_LEFT, Y_MID - BAR_RANGE, grid_w, 1},
                     theme->colors.divider);
    canvas_fill_rect(canvas, (Rect){GRID_LEFT, Y_MID - BAR_RANGE / 2, grid_w, 1},
                     theme->colors.divider);
    canvas_fill_rect(canvas, (Rect){GRID_LEFT - 4, Y_MID, grid_w + 8, 1},
                     theme->colors.text_secondary);
    canvas_fill_rect(canvas, (Rect){GRID_LEFT, Y_MID + BAR_RANGE / 2, grid_w, 1},
                     theme->colors.divider);
    canvas_fill_rect(canvas, (Rect){GRID_LEFT, Y_MID + BAR_RANGE, grid_w, 1},
                     theme->colors.divider);

    TextStyle scale_style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    label_draw(canvas, scale_style, (Label){.area = {8, Y_MID - BAR_RANGE - 6, 22, 12}, .text = "+12"});
    label_draw(canvas, scale_style, (Label){.area = {16, Y_MID - 6, 14, 12}, .text = "0"});
    label_draw(canvas, scale_style, (Label){.area = {8, Y_MID + BAR_RANGE - 6, 22, 12}, .text = "-12"});
}

static void draw_columns(const CustomEqScreen *screen, Canvas *canvas, const Theme *theme)
{
    const Settings *settings = &screen->store->settings;
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        int cx = BAND_CENTERS[band];
        int8_t gain = settings->eq_gains[band];
        int thumb_y = Y_MID - (gain * BAR_RANGE / EQ_GAIN_MAX);
        bool selected = band == screen->selected_band;

        canvas_fill_rect(canvas, (Rect){cx - TRACK_WIDTH / 2, Y_MID - BAR_RANGE, TRACK_WIDTH, BAR_RANGE * 2},
                         theme->colors.divider);

        Color fill_color = selected ? theme->colors.selection_top : theme->colors.text_tertiary;
        if (gain > 0) {
            canvas_fill_rect(canvas, (Rect){cx - FILL_WIDTH / 2, thumb_y, FILL_WIDTH, Y_MID - thumb_y},
                             fill_color);
        } else if (gain < 0) {
            canvas_fill_rect(canvas, (Rect){cx - FILL_WIDTH / 2, Y_MID, FILL_WIDTH, thumb_y - Y_MID},
                             fill_color);
        }

        Color thumb_color = selected
                                ? (screen->adjusting ? theme->colors.selection_top
                                                     : theme->colors.selection_background)
                                : theme->colors.text_secondary;
        canvas_fill_rect(canvas,
                         (Rect){cx - THUMB_WIDTH / 2, thumb_y - THUMB_HEIGHT / 2, THUMB_WIDTH, THUMB_HEIGHT},
                         thumb_color);
        if (selected) {
            canvas_fill_rect(
                canvas,
                (Rect){cx - THUMB_WIDTH / 2 + 1, thumb_y - THUMB_HEIGHT / 2 + 1, THUMB_WIDTH - 2, 1},
                theme->colors.selection_shine);
        }

        TextStyle freq_style = {
            .font = theme->fonts.small,
            .color = selected ? theme->colors.selection_background : theme->colors.text_secondary};
        Rect freq_area = label_centered_area(freq_style.font, (Rect){cx - 16, 134, 32, LABEL_HEIGHT},
                                            SHORT_NAMES[band]);
        label_draw(canvas, freq_style, (Label){.area = freq_area, .text = SHORT_NAMES[band]});
    }
}

static void draw(void *context, Canvas *canvas)
{
    CustomEqScreen *screen = context;
    const Theme *theme = screen->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, screen->screen.title);

    char title_buf[32];
    int8_t gain = screen->store->settings.eq_gains[screen->selected_band];
    format_band_title(screen->selected_band, gain, screen->adjusting, title_buf, sizeof title_buf);
    TextStyle title_style = {
        .font = theme->fonts.body,
        .color = screen->adjusting ? theme->colors.selection_background : theme->colors.text};
    Rect title_box = {0, 26, theme->metrics.screen_width, 16};
    Rect title_area = label_centered_area(title_style.font, title_box, title_buf);
    label_draw(canvas, title_style, (Label){.area = title_area, .text = title_buf});

    draw_grid(canvas, theme);
    draw_columns(screen, canvas, theme);

    const char *hint = screen->adjusting ? "UP/DOWN: Gain    PLAY: Done"
                                         : "PREV/NEXT: Band    PLAY: Adjust    BACK: Done";
    TextStyle hint_style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    Rect hint_box = {0, 152, theme->metrics.screen_width, 14};
    Rect hint_area = label_centered_area(hint_style.font, hint_box, hint);
    label_draw(canvas, hint_style, (Label){.area = hint_area, .text = hint});
}

Screen *custom_eq_screen_init(CustomEqScreen *custom_screen, SettingsStore *store,
                              const Theme *theme)
{
    custom_screen->store = store;
    custom_screen->theme = theme;
    custom_screen->selected_band = 0;
    custom_screen->adjusting = false;
    custom_screen->now_ms = 0;
    custom_screen->screen = (Screen){
        .title = "Custom EQ",
        .context = custom_screen,
        .full_screen = false,
        .paints_every_pixel = false,
        .hold_volume_keys = false,
        .stays_on_wake = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &custom_screen->screen;
}
