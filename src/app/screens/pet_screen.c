#include "pet_screen.h"

#include "core/string_builder.h"
#include "gfx/color.h"
#include "hal/display.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/navigation.h"

enum {
    CAT_BASE_WIDTH = 24,
    CAT_BASE_HEIGHT = 24,
    SPRITE_SCALE = 4,
    CAT_Y = 22,
    SHADOW_Y = 118,
    SHADOW_WIDTH = 72,
    SHADOW_HEIGHT = 2,
    TITLE_Y = 122,
    DESC_Y = 136,
    BAR_Y = 148,
    BAR_WIDTH = 180,
    BAR_HEIGHT = 5,
    TIME_Y = 156,
    LINE_HEIGHT = 14,
    TEXT_BUFFER_CAPACITY = 64
};

typedef struct {
    Color outline;
    Color fur_dark;
    Color fur_main;
    Color fur_light;
    Color pink;
    Color eye_iris;
    Color white;
    Color blue;
    Color blue_lt;
    Color red;
    Color gold;
} CatPalette;

static CatPalette get_palette(const Theme *theme)
{
    (void)theme;
    return (CatPalette){
        .outline = COLOR_RGB(22, 18, 28),
        .fur_dark = COLOR_RGB(150, 85, 30),
        .fur_main = COLOR_RGB(224, 120, 35),
        .fur_light = COLOR_RGB(255, 238, 215),
        .pink = COLOR_RGB(255, 120, 150),
        .eye_iris = COLOR_RGB(20, 160, 90),
        .white = COLOR_RGB(255, 255, 255),
        .blue = COLOR_RGB(45, 110, 220),
        .blue_lt = COLOR_RGB(120, 180, 255),
        .red = COLOR_RGB(228, 48, 64),
        .gold = COLOR_RGB(255, 204, 40),
    };
}

static Color get_palette_color(CatPalette pal, uint8_t idx)
{
    switch (idx) {
    case 1:
        return pal.outline;
    case 2:
        return pal.fur_dark;
    case 3:
        return pal.fur_main;
    case 4:
        return pal.fur_light;
    case 5:
        return pal.pink;
    case 6:
        return pal.eye_iris;
    case 7:
        return pal.white;
    case 8:
        return pal.blue;
    case 9:
        return pal.blue_lt;
    case 10:
        return pal.red;
    case 11:
        return pal.gold;
    default:
        return 0;
    }
}

static inline void draw_pixel(Canvas *canvas, int ox, int oy, int px, int py, Color color)
{
    canvas_fill_rect(canvas,
                     (Rect){ox + px * SPRITE_SCALE, oy + py * SPRITE_SCALE, SPRITE_SCALE,
                            SPRITE_SCALE},
                     color);
}

static inline void draw_span(Canvas *canvas, int ox, int oy, int px, int len, int py,
                             Color color)
{
    canvas_fill_rect(canvas,
                     (Rect){ox + px * SPRITE_SCALE, oy + py * SPRITE_SCALE, len * SPRITE_SCALE,
                            SPRITE_SCALE},
                     color);
}

static const uint8_t cat_sprite_egg[288] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x17, 0x74, 0x44, 0x41, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x77, 0x74, 0x44, 0x44, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x17, 0x74, 0x41, 0x44, 0x44, 0x41, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x77, 0x44, 0x11, 0x44, 0x44, 0x44, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x74, 0x44, 0x14, 0x44, 0x44, 0x44, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x17, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x44, 0x41, 0x14, 0x44, 0x41, 0x14, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x44, 0x51, 0x15, 0x44, 0x51, 0x15, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x44, 0x44, 0x45, 0x44, 0x54, 0x44, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x44, 0x44, 0x44, 0x54, 0x44, 0x44, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x44, 0x44, 0x45, 0x55, 0x44, 0x44, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x44, 0x44, 0x41, 0x41, 0x44, 0x44, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x44, 0x44, 0x33, 0x33, 0x33, 0x44, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x14, 0x43, 0x33, 0x33, 0x33, 0x31, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x14, 0x43, 0x33, 0x33, 0x33, 0x31, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x43, 0x33, 0x33, 0x33, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x13, 0x33, 0x33, 0x31, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t cat_sprite_baby[288] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x00, 0x01, 0x55, 0x41, 0x00, 0x00, 0x00, 0x00, 0x14, 0x55, 0x10, 0x00,
    0x00, 0x01, 0x55, 0x44, 0x10, 0x00, 0x00, 0x01, 0x44, 0x55, 0x10, 0x00,
    0x00, 0x01, 0x54, 0x44, 0x11, 0x11, 0x11, 0x11, 0x44, 0x45, 0x10, 0x00,
    0x00, 0x00, 0x14, 0x43, 0x33, 0x33, 0x33, 0x33, 0x34, 0x41, 0x00, 0x00,
    0x00, 0x00, 0x14, 0x33, 0x33, 0x22, 0x33, 0x22, 0x33, 0x41, 0x00, 0x00,
    0x00, 0x01, 0x43, 0x33, 0x33, 0x22, 0x33, 0x22, 0x33, 0x34, 0x10, 0x00,
    0x00, 0x01, 0x43, 0x31, 0x13, 0x33, 0x33, 0x33, 0x31, 0x14, 0x10, 0x00,
    0x00, 0x01, 0x43, 0x17, 0x71, 0x33, 0x33, 0x33, 0x17, 0x71, 0x41, 0x00,
    0x00, 0x01, 0x43, 0x16, 0x71, 0x33, 0x33, 0x33, 0x16, 0x71, 0x41, 0x00,
    0x00, 0x01, 0x43, 0x31, 0x13, 0x15, 0x11, 0x51, 0x31, 0x13, 0x41, 0x00,
    0x00, 0x01, 0x43, 0x33, 0x33, 0x43, 0x55, 0x34, 0x33, 0x33, 0x41, 0x00,
    0x00, 0x00, 0x14, 0x33, 0x33, 0x34, 0x55, 0x34, 0x33, 0x34, 0x10, 0x00,
    0x00, 0x00, 0x14, 0x33, 0x33, 0x33, 0x44, 0x33, 0x33, 0x34, 0x10, 0x00,
    0x00, 0x00, 0x01, 0x44, 0x33, 0x33, 0x33, 0x33, 0x33, 0x10, 0x00, 0x10,
    0x00, 0x00, 0x01, 0x41, 0x44, 0x44, 0x44, 0x44, 0x14, 0x10, 0x01, 0x41,
    0x00, 0x00, 0x14, 0x31, 0x44, 0x44, 0x44, 0x44, 0x13, 0x41, 0x14, 0x41,
    0x00, 0x00, 0x14, 0x31, 0x44, 0x44, 0x44, 0x44, 0x13, 0x41, 0x44, 0x10,
    0x00, 0x00, 0x13, 0x31, 0x44, 0x44, 0x44, 0x44, 0x13, 0x31, 0x41, 0x00,
    0x00, 0x00, 0x13, 0x31, 0x44, 0x44, 0x44, 0x44, 0x13, 0x31, 0x10, 0x00,
    0x00, 0x00, 0x01, 0x10, 0x11, 0x11, 0x11, 0x11, 0x01, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t cat_sprite_child[288] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x10, 0x00,
    0x00, 0x15, 0x54, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x45, 0x51, 0x00,
    0x00, 0x15, 0x54, 0x41, 0x00, 0x00, 0x00, 0x00, 0x14, 0x45, 0x51, 0x00,
    0x00, 0x15, 0x44, 0x41, 0x11, 0x11, 0x11, 0x11, 0x14, 0x44, 0x51, 0x00,
    0x00, 0x01, 0x44, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x44, 0x10, 0x00,
    0x00, 0x01, 0x43, 0x33, 0x22, 0x23, 0x32, 0x22, 0x33, 0x34, 0x10, 0x00,
    0x00, 0x14, 0x33, 0x33, 0x22, 0x23, 0x32, 0x22, 0x33, 0x33, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x31, 0x13, 0x33, 0x33, 0x31, 0x13, 0x33, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x17, 0x71, 0x33, 0x33, 0x17, 0x71, 0x33, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x16, 0x71, 0x33, 0x33, 0x16, 0x71, 0x33, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x31, 0x13, 0x51, 0x15, 0x31, 0x13, 0x33, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x33, 0x34, 0x35, 0x53, 0x43, 0x33, 0x33, 0x41, 0x00,
    0x00, 0x01, 0x43, 0x33, 0x33, 0x45, 0x53, 0x43, 0x33, 0x34, 0x10, 0x00,
    0x00, 0x01, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x10, 0x00,
    0x00, 0x00, 0x14, 0x43, 0x33, 0x33, 0x33, 0x33, 0x33, 0x31, 0x00, 0x10,
    0x00, 0x01, 0x44, 0x14, 0x44, 0x44, 0x44, 0x44, 0x44, 0x41, 0x01, 0x41,
    0x00, 0x14, 0x33, 0x14, 0x44, 0x44, 0x44, 0x44, 0x44, 0x41, 0x14, 0x41,
    0x00, 0x14, 0x33, 0x14, 0x42, 0x44, 0x44, 0x44, 0x24, 0x41, 0x44, 0x10,
    0x00, 0x14, 0x33, 0x14, 0x42, 0x44, 0x44, 0x44, 0x24, 0x41, 0x41, 0x00,
    0x00, 0x13, 0x33, 0x14, 0x42, 0x44, 0x44, 0x44, 0x24, 0x41, 0x10, 0x00,
    0x00, 0x01, 0x11, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t cat_sprite_adult[288] = {
    0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x00,
    0x01, 0x55, 0x54, 0x41, 0x00, 0x00, 0x00, 0x00, 0x14, 0x45, 0x55, 0x10,
    0x01, 0x55, 0x55, 0x44, 0x10, 0x00, 0x00, 0x01, 0x44, 0x55, 0x55, 0x10,
    0x01, 0x55, 0x44, 0x44, 0x11, 0x11, 0x11, 0x11, 0x44, 0x44, 0x55, 0x10,
    0x00, 0x14, 0x43, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x32, 0x22, 0x23, 0x32, 0x22, 0x23, 0x33, 0x41, 0x00,
    0x01, 0x43, 0x33, 0x32, 0x22, 0x23, 0x32, 0x22, 0x23, 0x33, 0x34, 0x10,
    0x01, 0x43, 0x33, 0x31, 0x13, 0x33, 0x33, 0x31, 0x13, 0x33, 0x34, 0x10,
    0x01, 0x43, 0x33, 0x17, 0x71, 0x33, 0x33, 0x17, 0x71, 0x33, 0x34, 0x10,
    0x01, 0x43, 0x33, 0x16, 0x71, 0x33, 0x33, 0x16, 0x71, 0x33, 0x34, 0x10,
    0x01, 0x43, 0x33, 0x31, 0x13, 0x51, 0x15, 0x31, 0x13, 0x33, 0x34, 0x10,
    0x01, 0x43, 0x33, 0x33, 0x43, 0x55, 0x34, 0x33, 0x33, 0x33, 0x34, 0x10,
    0x00, 0x14, 0x33, 0x33, 0x34, 0x55, 0x34, 0x33, 0x33, 0x33, 0x41, 0x00,
    0x00, 0x14, 0x33, 0x33, 0x33, 0x44, 0x33, 0x33, 0x33, 0x33, 0x41, 0x00,
    0x00, 0x01, 0xaa, 0xaa, 0xaa, 0xab, 0xbb, 0xaa, 0xaa, 0xaa, 0x10, 0x00,
    0x00, 0x01, 0xaa, 0xaa, 0xaa, 0xab, 0xbb, 0xaa, 0xaa, 0xaa, 0x10, 0x00,
    0x00, 0x00, 0x14, 0x43, 0x33, 0x33, 0x33, 0x33, 0x33, 0x31, 0x00, 0x10,
    0x00, 0x01, 0x44, 0x41, 0x44, 0x44, 0x44, 0x44, 0x44, 0x10, 0x01, 0x41,
    0x00, 0x14, 0x43, 0x31, 0x44, 0x44, 0x44, 0x44, 0x44, 0x10, 0x14, 0x41,
    0x00, 0x14, 0x33, 0x31, 0x44, 0x22, 0x44, 0x22, 0x44, 0x11, 0x44, 0x10,
    0x00, 0x14, 0x33, 0x31, 0x44, 0x22, 0x44, 0x22, 0x44, 0x14, 0x41, 0x00,
    0x00, 0x13, 0x33, 0x31, 0x44, 0x22, 0x44, 0x22, 0x44, 0x14, 0x10, 0x00,
    0x00, 0x01, 0x11, 0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void draw_bitmap(Canvas *canvas, int ox, int oy, const uint8_t *map, CatPalette pal)
{
    for (int r = 0; r < 24; r++) {
        int c = 0;
        while (c < 24) {
            uint8_t byte = map[r * 12 + c / 2];
            uint8_t color_idx = (c % 2 == 0) ? (byte >> 4) : (byte & 0x0Fu);
            if (color_idx == 0u) {
                c++;
                continue;
            }
            int start_c = c;
            c++;
            while (c < 24) {
                uint8_t next_b = map[r * 12 + c / 2];
                uint8_t next_idx = (c % 2 == 0) ? (next_b >> 4) : (next_b & 0x0Fu);
                if (next_idx != color_idx) {
                    break;
                }
                c++;
            }
            Color col = get_palette_color(pal, color_idx);
            draw_span(canvas, ox, oy, start_c, c - start_c, r, col);
        }
    }
}

static void draw_note(Canvas *canvas, int ox, int oy, int x, int y, Color c1, Color c2)
{
    draw_span(canvas, ox, oy, x + 1, 3, y, c1);
    draw_pixel(canvas, ox, oy, x + 3, y + 1, c1);
    draw_pixel(canvas, ox, oy, x + 3, y + 2, c1);
    draw_pixel(canvas, ox, oy, x + 1, y + 1, c1);
    draw_span(canvas, ox, oy, x, 2, y + 2, c1);
    draw_pixel(canvas, ox, oy, x + 1, y + 2, c2);
    draw_span(canvas, ox, oy, x + 2, 2, y + 3, c1);
    draw_pixel(canvas, ox, oy, x + 3, y + 3, c2);
}

static void draw_zzz(Canvas *canvas, int ox, int oy, int x, int y, int frame, Color c)
{
    int o1 = (frame == 1) ? -1 : 0;
    int o2 = (frame == 2) ? -1 : 0;
    draw_span(canvas, ox, oy, x + 3 + o2, 3, y + o2, c);
    draw_pixel(canvas, ox, oy, x + 4 + o2, y + 1 + o2, c);
    draw_span(canvas, ox, oy, x + 3 + o2, 3, y + 2 + o2, c);

    draw_span(canvas, ox, oy, x + o1, 2, y + 2 + o1, c);
    draw_pixel(canvas, ox, oy, x + 1 + o1, y + 3 + o1, c);
    draw_span(canvas, ox, oy, x + o1, 2, y + 4 + o1, c);
}

static void draw_heart(Canvas *canvas, int ox, int oy, int x, int y, Color c1, Color c2)
{
    draw_pixel(canvas, ox, oy, x, y, c1);
    draw_pixel(canvas, ox, oy, x + 2, y, c1);
    draw_span(canvas, ox, oy, x - 1, 5, y + 1, c1);
    draw_pixel(canvas, ox, oy, x, y + 1, c2);
    draw_span(canvas, ox, oy, x, 3, y + 2, c1);
    draw_pixel(canvas, ox, oy, x + 1, y + 3, c1);
}

static void draw_cat_overlays(Canvas *canvas, int x, int y, PetStage stage, PetState state,
                              uint32_t now_ms, CatPalette pal)
{
    bool blink = (now_ms % 3200u < 160u);

    if (stage == PET_STAGE_EGG) {
        if (state == PET_STATE_LISTENING) {
            int ny = (int)((now_ms / 300u) % 2u);
            draw_note(canvas, x, y, 19, 3 + ny, pal.blue, pal.blue_lt);
            draw_note(canvas, x, y, 1, 5 - ny, pal.blue, pal.blue_lt);
        } else if (state == PET_STATE_SLEEPING) {
            int zf = (int)((now_ms / 500u) % 3u);
            draw_zzz(canvas, x, y, 18, 1, zf, pal.outline);
        } else if (state == PET_STATE_REACTION) {
            int hp = ((now_ms / 150u) % 2u == 0u) ? 0 : 1;
            draw_heart(canvas, x, y, 17, hp, pal.red, pal.pink);
        }
        return;
    }

    if (state == PET_STATE_IDLE) {
        if (blink) {
            if (stage == PET_STAGE_ADULT) {
                draw_span(canvas, x, y, 5, 4, 9, pal.outline);
                draw_span(canvas, x, y, 15, 4, 9, pal.outline);
            } else {
                draw_span(canvas, x, y, 5, 4, 10, pal.outline);
                draw_span(canvas, x, y, 15, 4, 10, pal.outline);
            }
        }
    } else if (state == PET_STATE_LISTENING) {
        draw_span(canvas, x, y, 4, 16, 4, pal.blue);
        draw_span(canvas, x, y, 1, 3, 6, pal.blue);
        draw_span(canvas, x, y, 1, 3, 7, pal.blue);
        draw_span(canvas, x, y, 1, 3, 8, pal.blue);
        draw_pixel(canvas, x, y, 2, 7, pal.blue_lt);
        draw_span(canvas, x, y, 20, 3, 6, pal.blue);
        draw_span(canvas, x, y, 20, 3, 7, pal.blue);
        draw_span(canvas, x, y, 20, 3, 8, pal.blue);
        draw_pixel(canvas, x, y, 21, 7, pal.blue_lt);

        int ny = (int)((now_ms / 300u) % 2u);
        draw_note(canvas, x, y, 19, 1 + ny, pal.blue, pal.blue_lt);
    } else if (state == PET_STATE_SLEEPING) {
        if (stage == PET_STAGE_ADULT) {
            draw_span(canvas, x, y, 5, 4, 9, pal.outline);
            draw_span(canvas, x, y, 15, 4, 9, pal.outline);
        } else {
            draw_span(canvas, x, y, 5, 4, 10, pal.outline);
            draw_span(canvas, x, y, 15, 4, 10, pal.outline);
        }
        int zf = (int)((now_ms / 500u) % 3u);
        draw_zzz(canvas, x, y, 18, 1, zf, pal.outline);
    } else if (state == PET_STATE_REACTION) {
        draw_pixel(canvas, x, y, 5, 9, pal.outline);
        draw_pixel(canvas, x, y, 6, 8, pal.outline);
        draw_pixel(canvas, x, y, 7, 8, pal.outline);
        draw_pixel(canvas, x, y, 8, 9, pal.outline);
        draw_pixel(canvas, x, y, 15, 9, pal.outline);
        draw_pixel(canvas, x, y, 16, 8, pal.outline);
        draw_pixel(canvas, x, y, 17, 8, pal.outline);
        draw_pixel(canvas, x, y, 18, 9, pal.outline);

        draw_span(canvas, x, y, 4, 2, 11, pal.pink);
        draw_span(canvas, x, y, 18, 2, 11, pal.pink);

        int hp = ((now_ms / 150u) % 2u == 0u) ? 0 : 1;
        draw_heart(canvas, x, y, 17, hp, pal.red, pal.pink);
    }
}

static void draw_cat(Canvas *canvas, const Theme *theme, PetStage stage, PetState state,
                     uint32_t now_ms)
{
    (void)theme;
    CatPalette pal = get_palette(theme);

    const int cat_w = CAT_BASE_WIDTH * SPRITE_SCALE;
    int cat_x = (HAL_DISPLAY_WIDTH - cat_w) / 2;
    int cat_y = CAT_Y;

    if (stage == PET_STAGE_EGG) {
        if (state == PET_STATE_LISTENING) {
            int rock = ((now_ms / 280u) % 2u == 0u) ? -2 : 2;
            cat_x += rock;
        } else if (state == PET_STATE_REACTION) {
            int hop = ((now_ms / 180u) % 2u == 0u) ? -3 : 0;
            cat_y += hop;
        }
    } else {
        int bob = (state == PET_STATE_LISTENING && ((now_ms / 280u) % 2u == 0u)) ? 2 : 0;
        int hop = (state == PET_STATE_REACTION && ((now_ms / 180u) % 2u == 0u)) ? -2 : 0;
        cat_y += bob + hop;
    }

    const uint8_t *map = cat_sprite_egg;
    if (stage == PET_STAGE_BABY) {
        map = cat_sprite_baby;
    } else if (stage == PET_STAGE_CHILD) {
        map = cat_sprite_child;
    } else if (stage == PET_STAGE_ADULT) {
        map = cat_sprite_adult;
    }

    draw_bitmap(canvas, cat_x, cat_y, map, pal);
    draw_cat_overlays(canvas, cat_x, cat_y, stage, state, now_ms, pal);

    Color shadow_col = theme->colors.divider;
    canvas_fill_rect(canvas,
                     (Rect){(HAL_DISPLAY_WIDTH - SHADOW_WIDTH) / 2, SHADOW_Y, SHADOW_WIDTH,
                            SHADOW_HEIGHT},
                     shadow_col);
}

static const char *friendly_stage_name(PetStage stage)
{
    switch (stage) {
    case PET_STAGE_EGG:
        return "Cat Egg";
    case PET_STAGE_BABY:
        return "Little Kitten";
    case PET_STAGE_CHILD:
        return "Playful Cat";
    case PET_STAGE_ADULT:
        return "Companion Cat";
    default:
        return "EchoPet";
    }
}

static const char *friendly_status_description(PetState state)
{
    switch (state) {
    case PET_STATE_IDLE:
        return "Watching you curiously";
    case PET_STATE_LISTENING:
        return "Grooving to the music \xe2\x99\xaa";
    case PET_STATE_SLEEPING:
        return "Taking a sweet nap zZ";
    case PET_STATE_REACTION:
        return "Purring with joy! \xe2\x99\xa5";
    default:
        return "Resting peacefully";
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
    screen->now_ms = now_ms;
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
    uint32_t now_ms = screen->now_ms;

    header_draw(canvas, theme, screen->screen.title);

    draw_cat(canvas, theme, stage, state, now_ms);

    TextStyle title_style = {.font = theme->fonts.body, .color = theme->colors.text};
    TextStyle desc_style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    TextStyle time_style = {.font = theme->fonts.small, .color = theme->colors.text_tertiary};

    Rect full = {0, 0, HAL_DISPLAY_WIDTH, LINE_HEIGHT};
    const char *title_text = friendly_stage_name(stage);
    label_draw(canvas, title_style,
               (Label){.area = label_centered_area(title_style.font,
                                                   (Rect){0, TITLE_Y, full.width, full.height},
                                                   title_text),
                       .text = title_text});

    const char *desc_text = friendly_status_description(state);
    label_draw(canvas, desc_style,
               (Label){.area = label_centered_area(desc_style.font,
                                                   (Rect){0, DESC_Y, full.width, full.height},
                                                   desc_text),
                       .text = desc_text});

    int percent = pet_stage_progress_percent(&screen->pet.model);
    if (stage == PET_STAGE_ADULT) {
        TextStyle max_style = {.font = theme->fonts.small,
                               .color = theme->colors.selection_background};
        const char *max_text = "\xe2\x98\x85 Devoted Companion \xe2\x98\x85";
        label_draw(canvas, max_style,
                   (Label){.area = label_centered_area(max_style.font,
                                                       (Rect){0, BAR_Y - 2, full.width, LINE_HEIGHT},
                                                       max_text),
                           .text = max_text});
    } else {
        int bar_x = (HAL_DISPLAY_WIDTH - BAR_WIDTH) / 2;
        canvas_fill_rect(canvas, (Rect){bar_x - 1, BAR_Y - 1, BAR_WIDTH + 2, BAR_HEIGHT + 2},
                         theme->colors.divider);
        canvas_fill_rect(canvas, (Rect){bar_x, BAR_Y, BAR_WIDTH, BAR_HEIGHT},
                         theme->colors.background);
        int filled = (BAR_WIDTH * percent) / 100;
        if (filled > 0) {
            canvas_fill_rect(canvas, (Rect){bar_x, BAR_Y, filled, BAR_HEIGHT},
                             theme->colors.selection_background);
            canvas_fill_rect(canvas, (Rect){bar_x, BAR_Y, filled, 1},
                             theme->colors.selection_shine);
        }
    }

    char time_text[TEXT_BUFFER_CAPACITY];
    StringBuilder time_builder = string_builder_make(time_text, sizeof time_text);
    char duration[24];
    pet_format_duration(screen->pet.model.listening_time_ms, duration, sizeof duration);

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

    label_draw(canvas, time_style,
               (Label){.area = label_centered_area(time_style.font,
                                                   (Rect){0, TIME_Y, full.width, full.height},
                                                   time_text),
                       .text = time_text});
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
