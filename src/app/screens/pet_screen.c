#include "pet_screen.h"

#include "core/string_builder.h"
#include "gfx/color.h"
#include "hal/display.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    SPRITE_SCALE = 3,
    SPRITE_WIDTH = 16,
    SPRITE_HEIGHT = 16,
    CAT_Y = 26,
    TITLE_Y = 76,
    TIME_Y = 96,
    BAR_Y = 114,
    BAR_WIDTH = 160,
    BAR_HEIGHT = 6,
    DESC_Y = 126,
    HINT_Y = 146,
    LINE_HEIGHT = 18,
    TEXT_BUFFER_CAPACITY = 64
};

static void draw_span(Canvas *canvas, int x, int y, int col, int count, int row, Color color)
{
    canvas_fill_rect(canvas,
                     (Rect){x + col * SPRITE_SCALE, y + row * SPRITE_SCALE, count * SPRITE_SCALE,
                            SPRITE_SCALE},
                     color);
}

static void draw_pixel(Canvas *canvas, int x, int y, int col, int row, Color color)
{
    draw_span(canvas, x, y, col, 1, row, color);
}

static void draw_egg(Canvas *canvas, int x, int y, PetState state, Color fur, Color pink,
                     Color white, Color blue, Color red)
{
    draw_pixel(canvas, x, y, 4, 1, fur);
    draw_pixel(canvas, x, y, 11, 1, fur);
    draw_span(canvas, x, y, 4, 2, 2, fur);
    draw_pixel(canvas, x, y, 4, 2, pink);
    draw_span(canvas, x, y, 10, 2, 2, fur);
    draw_pixel(canvas, x, y, 11, 2, pink);

    draw_span(canvas, x, y, 5, 6, 3, fur);
    draw_span(canvas, x, y, 4, 8, 4, fur);
    draw_span(canvas, x, y, 3, 10, 5, fur);
    draw_span(canvas, x, y, 3, 10, 6, fur);
    draw_span(canvas, x, y, 3, 10, 7, fur);
    draw_span(canvas, x, y, 3, 10, 8, fur);
    draw_span(canvas, x, y, 3, 10, 9, fur);
    draw_span(canvas, x, y, 3, 10, 10, fur);
    draw_span(canvas, x, y, 4, 8, 11, fur);
    draw_span(canvas, x, y, 4, 8, 12, fur);
    draw_span(canvas, x, y, 5, 6, 13, fur);
    draw_span(canvas, x, y, 6, 4, 14, fur);

    draw_span(canvas, x, y, 5, 6, 5, white);
    draw_span(canvas, x, y, 4, 8, 6, white);
    draw_span(canvas, x, y, 4, 8, 7, white);
    draw_span(canvas, x, y, 4, 8, 8, white);
    draw_span(canvas, x, y, 4, 8, 9, white);
    draw_span(canvas, x, y, 5, 6, 10, white);

    draw_span(canvas, x, y, 7, 2, 8, pink);

    if (state == PET_STATE_IDLE) {
        draw_pixel(canvas, x, y, 5, 7, fur);
        draw_pixel(canvas, x, y, 10, 7, fur);
    } else if (state == PET_STATE_LISTENING) {
        draw_span(canvas, x, y, 5, 6, 3, blue);
        draw_span(canvas, x, y, 2, 2, 6, blue);
        draw_span(canvas, x, y, 2, 2, 7, blue);
        draw_span(canvas, x, y, 12, 2, 6, blue);
        draw_span(canvas, x, y, 12, 2, 7, blue);
        draw_pixel(canvas, x, y, 5, 7, fur);
        draw_pixel(canvas, x, y, 10, 7, fur);
        draw_pixel(canvas, x, y, 14, 0, blue);
        draw_span(canvas, x, y, 14, 2, 1, blue);
        draw_span(canvas, x, y, 13, 2, 2, blue);
    } else if (state == PET_STATE_SLEEPING) {
        draw_span(canvas, x, y, 5, 2, 7, fur);
        draw_span(canvas, x, y, 9, 2, 7, fur);
        draw_span(canvas, x, y, 13, 3, 0, fur);
        draw_pixel(canvas, x, y, 14, 1, fur);
        draw_span(canvas, x, y, 13, 3, 2, fur);
    } else if (state == PET_STATE_REACTION) {
        draw_pixel(canvas, x, y, 5, 7, fur);
        draw_pixel(canvas, x, y, 6, 6, fur);
        draw_pixel(canvas, x, y, 9, 6, fur);
        draw_pixel(canvas, x, y, 10, 7, fur);
        draw_pixel(canvas, x, y, 4, 8, pink);
        draw_pixel(canvas, x, y, 11, 8, pink);
        draw_pixel(canvas, x, y, 13, 0, red);
        draw_pixel(canvas, x, y, 15, 0, red);
        draw_span(canvas, x, y, 12, 4, 1, red);
        draw_span(canvas, x, y, 13, 3, 2, red);
        draw_pixel(canvas, x, y, 14, 3, red);
    }
}

static void draw_baby(Canvas *canvas, int x, int y, PetState state, Color fur, Color pink,
                      Color white, Color blue, Color red)
{
    draw_span(canvas, x, y, 3, 2, 0, fur);
    draw_span(canvas, x, y, 11, 2, 0, fur);

    draw_span(canvas, x, y, 2, 3, 1, fur);
    draw_pixel(canvas, x, y, 3, 1, pink);
    draw_span(canvas, x, y, 11, 3, 1, fur);
    draw_pixel(canvas, x, y, 12, 1, pink);

    draw_span(canvas, x, y, 2, 4, 2, fur);
    draw_span(canvas, x, y, 3, 2, 2, pink);
    draw_span(canvas, x, y, 10, 4, 2, fur);
    draw_span(canvas, x, y, 11, 2, 2, pink);

    draw_span(canvas, x, y, 3, 10, 3, fur);
    draw_span(canvas, x, y, 2, 12, 4, fur);
    draw_span(canvas, x, y, 2, 12, 5, fur);
    draw_span(canvas, x, y, 2, 12, 6, fur);
    draw_span(canvas, x, y, 2, 12, 7, fur);
    draw_span(canvas, x, y, 3, 10, 8, fur);

    draw_span(canvas, x, y, 4, 8, 9, fur);
    draw_span(canvas, x, y, 4, 8, 10, fur);
    draw_span(canvas, x, y, 4, 8, 11, fur);
    draw_span(canvas, x, y, 4, 8, 12, fur);
    draw_span(canvas, x, y, 3, 3, 13, fur);
    draw_span(canvas, x, y, 10, 3, 13, fur);
    draw_span(canvas, x, y, 3, 2, 14, fur);
    draw_span(canvas, x, y, 11, 2, 14, fur);

    draw_span(canvas, x, y, 7, 2, 7, pink);

    if (state == PET_STATE_IDLE) {
        draw_span(canvas, x, y, 4, 2, 5, white);
        draw_pixel(canvas, x, y, 5, 5, fur);
        draw_span(canvas, x, y, 10, 2, 5, white);
        draw_pixel(canvas, x, y, 10, 5, fur);
    } else if (state == PET_STATE_LISTENING) {
        draw_span(canvas, x, y, 5, 6, 2, blue);
        draw_span(canvas, x, y, 1, 2, 4, blue);
        draw_span(canvas, x, y, 1, 2, 5, blue);
        draw_span(canvas, x, y, 13, 2, 4, blue);
        draw_span(canvas, x, y, 13, 2, 5, blue);
        draw_span(canvas, x, y, 4, 2, 5, white);
        draw_pixel(canvas, x, y, 5, 5, fur);
        draw_span(canvas, x, y, 10, 2, 5, white);
        draw_pixel(canvas, x, y, 10, 5, fur);
        draw_pixel(canvas, x, y, 14, 0, blue);
        draw_span(canvas, x, y, 14, 2, 1, blue);
        draw_span(canvas, x, y, 13, 2, 2, blue);
    } else if (state == PET_STATE_SLEEPING) {
        draw_span(canvas, x, y, 4, 3, 5, white);
        draw_span(canvas, x, y, 9, 3, 5, white);
        draw_span(canvas, x, y, 13, 3, 0, fur);
        draw_pixel(canvas, x, y, 14, 1, fur);
        draw_span(canvas, x, y, 13, 3, 2, fur);
    } else if (state == PET_STATE_REACTION) {
        draw_pixel(canvas, x, y, 4, 5, white);
        draw_pixel(canvas, x, y, 5, 4, white);
        draw_pixel(canvas, x, y, 6, 5, white);
        draw_pixel(canvas, x, y, 9, 5, white);
        draw_pixel(canvas, x, y, 10, 4, white);
        draw_pixel(canvas, x, y, 11, 5, white);
        draw_span(canvas, x, y, 3, 2, 6, pink);
        draw_span(canvas, x, y, 11, 2, 6, pink);
        draw_pixel(canvas, x, y, 13, 0, red);
        draw_pixel(canvas, x, y, 15, 0, red);
        draw_span(canvas, x, y, 12, 4, 1, red);
        draw_span(canvas, x, y, 13, 3, 2, red);
        draw_pixel(canvas, x, y, 14, 3, red);
    }
}

static void draw_child(Canvas *canvas, int x, int y, PetState state, Color fur, Color pink,
                       Color white, Color blue, Color red)
{
    draw_span(canvas, x, y, 2, 2, 0, fur);
    draw_span(canvas, x, y, 12, 2, 0, fur);

    draw_span(canvas, x, y, 2, 3, 1, fur);
    draw_pixel(canvas, x, y, 3, 1, pink);
    draw_span(canvas, x, y, 11, 3, 1, fur);
    draw_pixel(canvas, x, y, 12, 1, pink);

    draw_span(canvas, x, y, 2, 4, 2, fur);
    draw_span(canvas, x, y, 3, 2, 2, pink);
    draw_span(canvas, x, y, 10, 4, 2, fur);
    draw_span(canvas, x, y, 11, 2, 2, pink);

    draw_span(canvas, x, y, 2, 12, 3, fur);
    draw_span(canvas, x, y, 1, 14, 4, fur);
    draw_span(canvas, x, y, 1, 14, 5, fur);
    draw_span(canvas, x, y, 1, 14, 6, fur);
    draw_span(canvas, x, y, 2, 12, 7, fur);

    draw_span(canvas, x, y, 3, 10, 8, fur);
    draw_span(canvas, x, y, 2, 12, 9, fur);
    draw_span(canvas, x, y, 2, 12, 10, fur);
    draw_span(canvas, x, y, 2, 12, 11, fur);
    draw_span(canvas, x, y, 2, 12, 12, fur);

    draw_span(canvas, x, y, 2, 3, 13, fur);
    draw_span(canvas, x, y, 6, 4, 13, fur);
    draw_span(canvas, x, y, 11, 3, 13, fur);

    draw_span(canvas, x, y, 2, 2, 14, fur);
    draw_span(canvas, x, y, 6, 4, 14, fur);
    draw_span(canvas, x, y, 12, 2, 14, fur);

    draw_span(canvas, x, y, 7, 2, 6, pink);

    draw_pixel(canvas, x, y, 0, 5, fur);
    draw_pixel(canvas, x, y, 0, 6, fur);
    draw_pixel(canvas, x, y, 15, 5, fur);
    draw_pixel(canvas, x, y, 15, 6, fur);

    if (state == PET_STATE_IDLE) {
        draw_span(canvas, x, y, 4, 2, 5, white);
        draw_pixel(canvas, x, y, 5, 5, fur);
        draw_span(canvas, x, y, 10, 2, 5, white);
        draw_pixel(canvas, x, y, 10, 5, fur);
    } else if (state == PET_STATE_LISTENING) {
        draw_span(canvas, x, y, 5, 6, 1, blue);
        draw_span(canvas, x, y, 0, 2, 4, blue);
        draw_span(canvas, x, y, 0, 2, 5, blue);
        draw_span(canvas, x, y, 0, 2, 6, blue);
        draw_pixel(canvas, x, y, 1, 5, white);
        draw_span(canvas, x, y, 14, 2, 4, blue);
        draw_span(canvas, x, y, 14, 2, 5, blue);
        draw_span(canvas, x, y, 14, 2, 6, blue);
        draw_pixel(canvas, x, y, 14, 5, white);
        draw_span(canvas, x, y, 4, 2, 5, white);
        draw_pixel(canvas, x, y, 5, 5, fur);
        draw_span(canvas, x, y, 10, 2, 5, white);
        draw_pixel(canvas, x, y, 10, 5, fur);
        draw_pixel(canvas, x, y, 14, 0, blue);
        draw_span(canvas, x, y, 14, 2, 1, blue);
        draw_span(canvas, x, y, 13, 2, 2, blue);
    } else if (state == PET_STATE_SLEEPING) {
        draw_span(canvas, x, y, 4, 3, 5, white);
        draw_span(canvas, x, y, 9, 3, 5, white);
        draw_span(canvas, x, y, 13, 3, 0, white);
        draw_pixel(canvas, x, y, 14, 1, white);
        draw_span(canvas, x, y, 13, 3, 2, white);
    } else if (state == PET_STATE_REACTION) {
        draw_pixel(canvas, x, y, 4, 5, white);
        draw_pixel(canvas, x, y, 5, 4, white);
        draw_pixel(canvas, x, y, 6, 5, white);
        draw_pixel(canvas, x, y, 9, 5, white);
        draw_pixel(canvas, x, y, 10, 4, white);
        draw_pixel(canvas, x, y, 11, 5, white);
        draw_span(canvas, x, y, 3, 2, 6, pink);
        draw_span(canvas, x, y, 11, 2, 6, pink);
        draw_pixel(canvas, x, y, 13, 0, red);
        draw_pixel(canvas, x, y, 15, 0, red);
        draw_span(canvas, x, y, 12, 4, 1, red);
        draw_span(canvas, x, y, 13, 3, 2, red);
        draw_pixel(canvas, x, y, 14, 3, red);
    }
}

static void draw_adult(Canvas *canvas, int x, int y, PetState state, Color fur, Color pink,
                       Color white, Color blue, Color red)
{
    draw_span(canvas, x, y, 1, 3, 0, fur);
    draw_span(canvas, x, y, 12, 3, 0, fur);

    draw_span(canvas, x, y, 1, 4, 1, fur);
    draw_span(canvas, x, y, 2, 2, 1, pink);
    draw_span(canvas, x, y, 11, 4, 1, fur);
    draw_span(canvas, x, y, 12, 2, 1, pink);

    draw_span(canvas, x, y, 1, 5, 2, fur);
    draw_span(canvas, x, y, 2, 2, 2, pink);
    draw_span(canvas, x, y, 10, 5, 2, fur);
    draw_span(canvas, x, y, 12, 2, 2, pink);

    draw_span(canvas, x, y, 2, 12, 3, fur);
    draw_span(canvas, x, y, 0, 16, 4, fur);
    draw_span(canvas, x, y, 0, 16, 5, fur);
    draw_span(canvas, x, y, 1, 14, 6, fur);
    draw_span(canvas, x, y, 2, 12, 7, fur);

    draw_span(canvas, x, y, 3, 10, 8, blue);
    draw_span(canvas, x, y, 7, 2, 9, red);

    draw_span(canvas, x, y, 2, 12, 9, fur);
    draw_span(canvas, x, y, 7, 2, 9, red);
    draw_span(canvas, x, y, 2, 12, 10, fur);
    draw_span(canvas, x, y, 1, 14, 11, fur);
    draw_span(canvas, x, y, 1, 14, 12, fur);

    draw_span(canvas, x, y, 1, 4, 13, fur);
    draw_span(canvas, x, y, 6, 4, 13, fur);
    draw_span(canvas, x, y, 11, 4, 13, fur);

    draw_span(canvas, x, y, 1, 3, 14, fur);
    draw_span(canvas, x, y, 6, 4, 14, fur);
    draw_span(canvas, x, y, 12, 3, 14, fur);

    draw_span(canvas, x, y, 7, 2, 6, pink);

    draw_span(canvas, x, y, 0, 2, 6, white);
    draw_span(canvas, x, y, 14, 2, 6, white);

    if (state == PET_STATE_IDLE) {
        draw_span(canvas, x, y, 4, 3, 5, white);
        draw_span(canvas, x, y, 5, 2, 5, fur);
        draw_span(canvas, x, y, 9, 3, 5, white);
        draw_span(canvas, x, y, 9, 2, 5, fur);
    } else if (state == PET_STATE_LISTENING) {
        draw_span(canvas, x, y, 4, 8, 1, blue);
        draw_span(canvas, x, y, 0, 2, 3, blue);
        draw_span(canvas, x, y, 0, 2, 4, blue);
        draw_span(canvas, x, y, 0, 2, 5, blue);
        draw_pixel(canvas, x, y, 0, 4, white);
        draw_span(canvas, x, y, 14, 2, 3, blue);
        draw_span(canvas, x, y, 14, 2, 4, blue);
        draw_span(canvas, x, y, 14, 2, 5, blue);
        draw_pixel(canvas, x, y, 15, 4, white);
        draw_span(canvas, x, y, 4, 3, 5, white);
        draw_span(canvas, x, y, 5, 2, 5, fur);
        draw_span(canvas, x, y, 9, 3, 5, white);
        draw_span(canvas, x, y, 9, 2, 5, fur);
        draw_pixel(canvas, x, y, 14, 0, blue);
        draw_span(canvas, x, y, 14, 2, 1, blue);
        draw_span(canvas, x, y, 13, 2, 2, blue);
    } else if (state == PET_STATE_SLEEPING) {
        draw_span(canvas, x, y, 4, 3, 5, white);
        draw_span(canvas, x, y, 9, 3, 5, white);
        draw_span(canvas, x, y, 13, 3, 0, white);
        draw_pixel(canvas, x, y, 14, 1, white);
        draw_span(canvas, x, y, 13, 3, 2, white);
    } else if (state == PET_STATE_REACTION) {
        draw_pixel(canvas, x, y, 4, 5, white);
        draw_pixel(canvas, x, y, 5, 4, white);
        draw_pixel(canvas, x, y, 6, 5, white);
        draw_pixel(canvas, x, y, 9, 5, white);
        draw_pixel(canvas, x, y, 10, 4, white);
        draw_pixel(canvas, x, y, 11, 5, white);
        draw_span(canvas, x, y, 3, 2, 6, pink);
        draw_span(canvas, x, y, 11, 2, 6, pink);
        draw_pixel(canvas, x, y, 13, 0, red);
        draw_pixel(canvas, x, y, 15, 0, red);
        draw_span(canvas, x, y, 12, 4, 1, red);
        draw_span(canvas, x, y, 13, 3, 2, red);
        draw_pixel(canvas, x, y, 14, 3, red);
    }
}

static void draw_cat(Canvas *canvas, const Theme *theme, PetStage stage, PetState state)
{
    int cat_w = SPRITE_WIDTH * SPRITE_SCALE;
    int cat_x = (HAL_DISPLAY_WIDTH - cat_w) / 2;
    int cat_y = CAT_Y;

    Color fur = theme->colors.text;
    Color pink = COLOR_RGB(250, 140, 160);
    Color white = theme->colors.background;
    Color blue = theme->colors.selection_background;
    Color red = COLOR_RGB(235, 60, 75);

    switch (stage) {
    case PET_STAGE_EGG:
        draw_egg(canvas, cat_x, cat_y, state, fur, pink, white, blue, red);
        break;
    case PET_STAGE_BABY:
        draw_baby(canvas, cat_x, cat_y, state, fur, pink, white, blue, red);
        break;
    case PET_STAGE_CHILD:
        draw_child(canvas, cat_x, cat_y, state, fur, pink, white, blue, red);
        break;
    case PET_STAGE_ADULT:
        draw_adult(canvas, cat_x, cat_y, state, fur, pink, white, blue, red);
        break;
    }
}

static void enter(void *context, uint32_t now_ms)
{
    PetScreen *screen = context;
    PlayerStatus status =
        screen->player != NULL ? player_status(screen->player) : (PlayerStatus){0};
    pet_set_screen_active(&screen->pet, true, status, now_ms);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    PetScreen *screen = context;
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        pet_screen_interact(screen, now_ms);
        return screen_action_none();
    }
    ScreenAction exit_action = navigation_exit_action(event);
    if (exit_action.type != SCREEN_ACTION_NONE) {
        PlayerStatus status =
            screen->player != NULL ? player_status(screen->player) : (PlayerStatus){0};
        pet_set_screen_active(&screen->pet, false, status, now_ms);
        pet_save(&screen->pet);
    }
    return exit_action;
}

static void update(void *context, uint32_t now_ms)
{
    PetScreen *screen = context;
    PlayerStatus status =
        screen->player != NULL ? player_status(screen->player) : (PlayerStatus){0};
    pet_update(&screen->pet, status, now_ms);
}

static void draw(void *context, Canvas *canvas)
{
    PetScreen *screen = context;
    const Theme *theme = screen->theme;
    PetStage stage = screen->pet.model.stage;
    PetState state = pet_state(&screen->pet);

    header_draw(canvas, theme, screen->screen.title);
    draw_cat(canvas, theme, stage, state);

    char title_text[TEXT_BUFFER_CAPACITY];
    StringBuilder title_builder = string_builder_make(title_text, sizeof title_text);
    string_builder_append(&title_builder, pet_stage_name(stage));
    string_builder_append(&title_builder, " • ");
    string_builder_append(&title_builder, pet_state_name(state));

    char time_text[TEXT_BUFFER_CAPACITY];
    StringBuilder time_builder = string_builder_make(time_text, sizeof time_text);
    char duration[24];
    pet_format_duration(screen->pet.model.listening_time_ms, duration, sizeof duration);

    int percent = pet_stage_progress_percent(&screen->pet.model);
    if (stage == PET_STAGE_ADULT) {
        string_builder_append(&time_builder, "Total listened: ");
        string_builder_append(&time_builder, duration);
    } else {
        char target[24];
        pet_format_duration(pet_next_stage_threshold(stage), target, sizeof target);
        string_builder_append(&time_builder, duration);
        string_builder_append(&time_builder, " / ");
        string_builder_append(&time_builder, target);
        string_builder_append(&time_builder, " (");
        string_builder_append_unsigned(&time_builder, (uint32_t)percent);
        string_builder_append(&time_builder, "%)");
    }

    const char *desc_text = pet_state_description(state);
    const char *hint_text = "Play: Interact   Back: Exit";

    TextStyle title_style = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle time_style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    TextStyle desc_style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    TextStyle hint_style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};

    Rect full = {0, 0, HAL_DISPLAY_WIDTH, LINE_HEIGHT};
    label_draw(canvas, title_style,
               (Label){.area = label_centered_area(title_style.font,
                                                   (Rect){0, TITLE_Y, full.width, full.height},
                                                   title_text),
                       .text = title_text});
    label_draw(canvas, time_style,
               (Label){.area = label_centered_area(time_style.font,
                                                   (Rect){0, TIME_Y, full.width, full.height},
                                                   time_text),
                       .text = time_text});

    if (stage == PET_STAGE_ADULT) {
        TextStyle max_style = {.font = theme->fonts.small,
                               .color = theme->colors.selection_background};
        const char *max_text = "★ Adult Companion ★";
        label_draw(canvas, max_style,
                   (Label){.area = label_centered_area(max_style.font,
                                                       (Rect){0, BAR_Y, full.width, LINE_HEIGHT},
                                                       max_text),
                           .text = max_text});
    } else {
        int bar_x = (HAL_DISPLAY_WIDTH - BAR_WIDTH) / 2;
        canvas_fill_rect(canvas, (Rect){bar_x, BAR_Y, BAR_WIDTH, BAR_HEIGHT}, theme->colors.divider);
        int filled = (BAR_WIDTH * percent) / 100;
        if (filled > 0) {
            canvas_fill_rect(canvas, (Rect){bar_x, BAR_Y, filled, BAR_HEIGHT},
                             theme->colors.selection_background);
        }
    }

    label_draw(canvas, desc_style,
               (Label){.area = label_centered_area(desc_style.font,
                                                   (Rect){0, DESC_Y, full.width, full.height},
                                                   desc_text),
                       .text = desc_text});
    label_draw(canvas, hint_style,
               (Label){.area = label_centered_area(hint_style.font,
                                                   (Rect){0, HINT_Y, full.width, full.height},
                                                   hint_text),
                       .text = hint_text});
}

Screen *pet_screen_init(PetScreen *screen, const Theme *theme, const Player *player)
{
    *screen = (PetScreen){.theme = theme, .player = player};
    pet_init(&screen->pet);
    screen->screen = (Screen){
        .title = "EchoPet",
        .stays_on_wake = true,
        .context = screen,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &screen->screen;
}

Pet *pet_screen_pet(PetScreen *screen)
{
    return &screen->pet;
}

void pet_screen_interact(PetScreen *screen, uint32_t now_ms)
{
    pet_interact(&screen->pet, now_ms);
}
