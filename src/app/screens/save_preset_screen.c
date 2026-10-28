#include "screens/save_preset_screen.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "ui/header.h"
#include "ui/label.h"

static void init_default_name(SavePresetScreen *screen)
{
    const Settings *settings = &screen->store->settings;
    for (int num = 1; num <= EQ_USER_PRESET_CAPACITY + 1; num++) {
        char candidate[EQ_USER_PRESET_NAME_MAX + 1];
        StringBuilder b = string_builder_make(candidate, sizeof candidate);
        string_builder_append(&b, "Preset ");
        string_builder_append_unsigned(&b, (uint32_t)num);
        if (settings_find_user_preset_by_name(settings, candidate) < 0) {
            int char_idx = 0;
            while (candidate[char_idx] != '\0') {
                screen->name_buffer[char_idx] = candidate[char_idx];
                char_idx++;
            }
            screen->name_buffer[char_idx] = '\0';
            return;
        }
    }
    screen->name_buffer[0] = 'P';
    screen->name_buffer[1] = '1';
    screen->name_buffer[2] = '\0';
}

static char next_char(char c)
{
    if (c >= 'A' && c < 'Z') {
        return (char)(c + 1);
    }
    if (c == 'Z') {
        return 'a';
    }
    if (c >= 'a' && c < 'z') {
        return (char)(c + 1);
    }
    if (c == 'z') {
        return '0';
    }
    if (c >= '0' && c < '9') {
        return (char)(c + 1);
    }
    if (c == '9') {
        return ' ';
    }
    if (c == ' ') {
        return '-';
    }
    return 'A';
}

static char prev_char(char c)
{
    if (c > 'A' && c <= 'Z') {
        return (char)(c - 1);
    }
    if (c == 'A') {
        return '-';
    }
    if (c == '-') {
        return ' ';
    }
    if (c == ' ') {
        return '9';
    }
    if (c > '0' && c <= '9') {
        return (char)(c - 1);
    }
    if (c == '0') {
        return 'z';
    }
    if (c > 'a' && c <= 'z') {
        return (char)(c - 1);
    }
    if (c == 'a') {
        return 'Z';
    }
    return 'Z';
}

static ScreenAction handle_editing_key(SavePresetScreen *screen, KeyEvent event)
{
    bool is_step = event.gesture == KEY_GESTURE_PRESS || event.gesture == KEY_GESTURE_REPEAT;
    if (event.key == KEY_VOLUME_UP && is_step) {
        char cur = screen->name_buffer[screen->char_cursor];
        screen->name_buffer[screen->char_cursor] = next_char(cur);
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_DOWN && is_step) {
        char cur = screen->name_buffer[screen->char_cursor];
        screen->name_buffer[screen->char_cursor] = prev_char(cur);
        return screen_action_none();
    }
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        if (screen->name_buffer[screen->char_cursor] == '\0') {
            screen->name_buffer[screen->char_cursor] = 'A';
            screen->name_buffer[screen->char_cursor + 1] = '\0';
        }
        screen->char_cursor++;
        if (screen->char_cursor >= EQ_USER_PRESET_NAME_MAX ||
            screen->name_buffer[screen->char_cursor] == '\0') {
            screen->editing_name = false;
            screen->char_cursor = 0;
        }
        return screen_action_none();
    }
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        screen->editing_name = false;
        screen->char_cursor = 0;
        return screen_action_none();
    }
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    SavePresetScreen *screen = context;
    screen->now_ms = now_ms;
    if (screen->editing_name) {
        return handle_editing_key(screen, event);
    }
    bool is_step = event.gesture == KEY_GESTURE_PRESS || event.gesture == KEY_GESTURE_REPEAT;
    if (event.key == KEY_VOLUME_DOWN && is_step) {
        if (screen->selected_row > 0) {
            screen->selected_row--;
        }
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_UP && is_step) {
        if (screen->selected_row < SAVE_PRESET_ROW_COUNT - 1) {
            screen->selected_row++;
        }
        return screen_action_none();
    }
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        if (screen->selected_row == SAVE_PRESET_ROW_SAVE) {
            settings_save_user_preset(&screen->store->settings, screen->name_buffer,
                                      screen->store->settings.eq_gains);
            settings_store_save(screen->store);
            return screen_action_pop();
        }
        if (screen->selected_row == SAVE_PRESET_ROW_EDIT) {
            screen->editing_name = true;
            screen->char_cursor = 0;
            return screen_action_none();
        }
        if (screen->selected_row == SAVE_PRESET_ROW_CANCEL) {
            return screen_action_pop();
        }
    }
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        return screen_action_pop();
    }
    return screen_action_none();
}

static void enter(void *context, uint32_t now_ms)
{
    SavePresetScreen *screen = context;
    screen->now_ms = now_ms;
    screen->editing_name = false;
    screen->char_cursor = 0;
    screen->selected_row = SAVE_PRESET_ROW_SAVE;
    init_default_name(screen);
}

static void update(void *context, uint32_t now_ms)
{
    SavePresetScreen *screen = context;
    screen->now_ms = now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    SavePresetScreen *screen = context;
    const Theme *theme = screen->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, screen->screen.title);

    Rect box_rect = {20, 32, theme->metrics.screen_width - 40, 26};
    canvas_fill_rect(canvas, box_rect, theme->colors.header_top);
    canvas_fill_rect(canvas, (Rect){box_rect.x, box_rect.y + box_rect.height - 1, box_rect.width, 1},
                     theme->colors.divider);

    char label_buf[32];
    StringBuilder b = string_builder_make(label_buf, sizeof label_buf);
    string_builder_append(&b, "Name: ");
    string_builder_append(&b, screen->name_buffer);
    TextStyle text_style = {.font = theme->fonts.body, .color = theme->colors.text};
    Rect name_area = label_centered_area(text_style.font, box_rect, label_buf);
    label_draw(canvas, text_style, (Label){.area = name_area, .text = label_buf});

    if (screen->editing_name) {
        canvas_fill_rect(canvas, (Rect){name_area.x + name_area.width - 2, name_area.y + name_area.height - 2, 8, 2},
                         theme->colors.selection_background);
    }

    static const char *const ROW_LABELS[SAVE_PRESET_ROW_COUNT] = {"Save Preset", "Edit Name",
                                                                 "Cancel"};
    int start_y = 66;
    int row_h = theme->metrics.row_height;
    for (int row = 0; row < SAVE_PRESET_ROW_COUNT; row++) {
        bool selected = !screen->editing_name && (row == screen->selected_row);
        Rect row_rect = {10, start_y + row * row_h, theme->metrics.screen_width - 20, row_h};
        if (selected) {
            canvas_fill_rect(canvas, row_rect, theme->colors.selection_background);
        }
        TextStyle row_style = {
            .font = theme->fonts.body,
            .color = selected ? theme->colors.selection_text : theme->colors.text};
        Rect label_rect = {row_rect.x + 10, row_rect.y, row_rect.width - 20, row_rect.height};
        Rect centered = label_centered_area(row_style.font, label_rect, ROW_LABELS[row]);
        label_draw(canvas, row_style, (Label){.area = centered, .text = ROW_LABELS[row]});
    }

    const char *hint = screen->editing_name ? "UP/DOWN: Char    PLAY: Next    BACK: Done"
                                           : "UP/DOWN: Move    PLAY: Select    BACK: Exit";
    TextStyle hint_style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};
    Rect hint_box = {0, 150, theme->metrics.screen_width, 14};
    Rect hint_area = label_centered_area(hint_style.font, hint_box, hint);
    label_draw(canvas, hint_style, (Label){.area = hint_area, .text = hint});
}

Screen *save_preset_screen_init(SavePresetScreen *save_screen, SettingsStore *store,
                                const Theme *theme)
{
    save_screen->store = store;
    save_screen->theme = theme;
    save_screen->selected_row = SAVE_PRESET_ROW_SAVE;
    save_screen->editing_name = false;
    save_screen->char_cursor = 0;
    save_screen->now_ms = 0;
    save_screen->name_buffer[0] = '\0';
    save_screen->screen = (Screen){
        .title = "Save Preset",
        .context = save_screen,
        .full_screen = false,
        .paints_every_pixel = false,
        .hold_volume_keys = false,
        .stays_on_wake = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &save_screen->screen;
}
