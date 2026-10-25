#include "gfx/font.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    LEVELS_PER_BYTE = 2,
    HIGH_NIBBLE_SHIFT = 4,
    NIBBLE_MASK = 0x0F
};

bool font_is_cjk(uint32_t codepoint)
{
    return (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
           (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||
           (codepoint >= 0x3040 && codepoint <= 0x309F) ||
           (codepoint >= 0x30A0 && codepoint <= 0x30FF) ||
           (codepoint >= 0x3000 && codepoint <= 0x303F) ||
           (codepoint >= 0x31F0 && codepoint <= 0x31FF) ||
           (codepoint >= 0x3200 && codepoint <= 0x32FF) ||
           (codepoint >= 0x3300 && codepoint <= 0x33FF) ||
           (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||
           (codepoint >= 0xFF00 && codepoint <= 0xFFEF) ||
           (codepoint >= 0x2E80 && codepoint <= 0x2FDF) ||
           (codepoint >= 0x20000 && codepoint <= 0x2FA1F);
}

static const Glyph *search(const Font *font, uint32_t codepoint)
{
    int low = 0;
    int high = (int)font->glyph_count - 1;
    while (low <= high) {
        int middle = low + (high - low) / 2;
        uint32_t found = font->glyphs[middle].codepoint;
        if (found == codepoint) {
            return &font->glyphs[middle];
        }
        if (found < codepoint) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return NULL;
}

const Glyph *font_find_glyph(const Font *font, uint32_t codepoint)
{
    const Glyph *glyph = search(font, codepoint);
    if (glyph != NULL) {
        return glyph;
    }
    if (font_is_cjk(codepoint)) {
        const Glyph *cjk = search(font, FONT_CJK_FALLBACK_CODEPOINT);
        if (cjk != NULL) {
            return cjk;
        }
    }
    return search(font, FONT_FALLBACK_CODEPOINT);
}

uint8_t font_glyph_level(const Font *font, const Glyph *glyph, int x, int y)
{
    int index = y * glyph->width + x;
    uint8_t packed = font->bitmap[glyph->bitmap_offset + index / LEVELS_PER_BYTE];
    bool high_nibble = index % LEVELS_PER_BYTE == 0;
    return (uint8_t)(high_nibble ? packed >> HIGH_NIBBLE_SHIFT : packed & NIBBLE_MASK);
}

int font_line_height(const Font *font)
{
    return font->ascent + font->descent;
}
