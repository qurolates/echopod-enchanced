#include "pet_screen.h"

#include "gfx/color.h"
#include "hal/display.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    SPRITE_SCALE = 3,
    SPRITE_WIDTH = 16,
    SPRITE_HEIGHT = 16,
    CAT_Y = 28,
    STATE_NAME_Y = 82,
    STATE_DESC_Y = 106,
    HINT_Y = 136,
    LINE_HEIGHT = 20
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

static void draw_cat_body(Canvas *canvas, int x, int y, Color fur, Color pink)
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
}

static void draw_idle_features(Canvas *canvas, int x, int y, Color eye_white, Color pupil)
{
    draw_span(canvas, x, y, 4, 2, 5, eye_white);
    draw_pixel(canvas, x, y, 5, 5, pupil);
    draw_span(canvas, x, y, 10, 2, 5, eye_white);
    draw_pixel(canvas, x, y, 10, 5, pupil);
}

static void draw_listening_features(Canvas *canvas, int x, int y, Color blue, Color white,
                                    Color pupil)
{
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
    draw_pixel(canvas, x, y, 5, 5, pupil);
    draw_span(canvas, x, y, 10, 2, 5, white);
    draw_pixel(canvas, x, y, 10, 5, pupil);

    draw_pixel(canvas, x, y, 14, 0, blue);
    draw_span(canvas, x, y, 14, 2, 1, blue);
    draw_span(canvas, x, y, 13, 2, 2, blue);
}

static void draw_sleeping_features(Canvas *canvas, int x, int y, Color dark, Color light)
{
    draw_span(canvas, x, y, 4, 3, 5, dark);
    draw_span(canvas, x, y, 9, 3, 5, dark);

    draw_span(canvas, x, y, 13, 3, 0, light);
    draw_pixel(canvas, x, y, 14, 1, light);
    draw_span(canvas, x, y, 13, 3, 2, light);
}

static void draw_reaction_features(Canvas *canvas, int x, int y, Color dark, Color pink, Color red)
{
    draw_pixel(canvas, x, y, 4, 5, dark);
    draw_pixel(canvas, x, y, 5, 4, dark);
    draw_pixel(canvas, x, y, 6, 5, dark);

    draw_pixel(canvas, x, y, 9, 5, dark);
    draw_pixel(canvas, x, y, 10, 4, dark);
    draw_pixel(canvas, x, y, 11, 5, dark);

    draw_span(canvas, x, y, 3, 2, 6, pink);
    draw_span(canvas, x, y, 11, 2, 6, pink);

    draw_pixel(canvas, x, y, 13, 0, red);
    draw_pixel(canvas, x, y, 15, 0, red);
    draw_span(canvas, x, y, 12, 4, 1, red);
    draw_span(canvas, x, y, 13, 3, 2, red);
    draw_pixel(canvas, x, y, 14, 3, red);
}

static void draw_cat(Canvas *canvas, const Theme *theme, PetState state)
{
    int cat_w = SPRITE_WIDTH * SPRITE_SCALE;
    int cat_x = (HAL_DISPLAY_WIDTH - cat_w) / 2;
    int cat_y = CAT_Y;

    Color fur = theme->colors.text;
    Color pink = COLOR_RGB(250, 140, 160);
    Color white = theme->colors.background;
    Color dark = theme->colors.background;
    Color blue = theme->colors.selection_background;
    Color red = COLOR_RGB(235, 60, 75);

    draw_cat_body(canvas, cat_x, cat_y, fur, pink);

    switch (state) {
    case PET_STATE_IDLE:
        draw_idle_features(canvas, cat_x, cat_y, white, fur);
        break;
    case PET_STATE_LISTENING:
        draw_listening_features(canvas, cat_x, cat_y, blue, white, fur);
        break;
    case PET_STATE_SLEEPING:
        draw_sleeping_features(canvas, cat_x, cat_y, dark, theme->colors.text_secondary);
        break;
    case PET_STATE_REACTION:
        draw_reaction_features(canvas, cat_x, cat_y, dark, pink, red);
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
    PetState state = pet_state(&screen->pet);

    header_draw(canvas, theme, screen->screen.title);
    draw_cat(canvas, theme, state);

    const char *state_text = pet_state_name(state);
    const char *desc_text = pet_state_description(state);
    const char *hint_text = "Play: Interact   Back: Exit";

    TextStyle state_style = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle desc_style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    TextStyle hint_style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};

    Rect full = {0, 0, HAL_DISPLAY_WIDTH, LINE_HEIGHT};
    label_draw(canvas, state_style,
               (Label){.area = label_centered_area(state_style.font,
                                                   (Rect){0, STATE_NAME_Y, full.width, full.height},
                                                   state_text),
                       .text = state_text});
    label_draw(canvas, desc_style,
               (Label){.area = label_centered_area(desc_style.font,
                                                   (Rect){0, STATE_DESC_Y, full.width, full.height},
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
