#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Glyph {
    uint16_t codepoint;
    uint16_t bitmap_offset;
    uint8_t width;
    uint8_t height;
    int8_t left;
    int8_t top;
    uint8_t advance;
} Glyph;

typedef struct Font {
    const Glyph *glyphs;
    const uint8_t *bitmap;
    uint16_t glyph_count;
    uint16_t bitmap_size;
    uint8_t ascent;
    uint8_t descent;
} Font;

enum {
    FONT_FALLBACK_CODEPOINT = '?',
    FONT_CJK_FALLBACK_CODEPOINT = 0x25A1
};

bool font_is_cjk(uint32_t codepoint);
const Glyph *font_find_glyph(const Font *font, uint32_t codepoint);
uint8_t font_glyph_level(const Font *font, const Glyph *glyph, int x, int y);
int font_line_height(const Font *font);
