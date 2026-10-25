#include <stddef.h>
#include <string.h>

#include "check.h"
#include "fonts/fonts.h"
#include "gfx/text.h"
#include "ui/label.h"
#include "ui/marquee.h"

static const Font *const ALL_FONTS[] = {
    &FONT_INTER_REGULAR_12,
    &FONT_INTER_REGULAR_16,
    &FONT_INTER_SEMIBOLD_16,
};

enum {
    FONT_COUNT = sizeof ALL_FONTS / sizeof ALL_FONTS[0]
};

static void every_font_covers_printable_ascii(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        for (uint32_t codepoint = ' '; codepoint <= '~'; codepoint++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], codepoint);
            CHECK(glyph != NULL && glyph->codepoint == codepoint);
        }
    }
}

static void every_font_covers_western_european_accents(void)
{
    const uint32_t accented[] = {0xE9,   0xF6,   0xF8,   0xC5,   0xF1,  0xDF,
                                 0x2026, 0x2019, 0x203A, 0x2605, 0x2713};
    for (size_t font = 0; font < FONT_COUNT; font++) {
        for (size_t index = 0; index < sizeof accented / sizeof accented[0]; index++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], accented[index]);
            CHECK(glyph != NULL && glyph->codepoint == accented[index]);
        }
    }
}

static void glyphs_are_sorted_by_codepoint(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        const Font *current = ALL_FONTS[font];
        for (uint16_t index = 1; index < current->glyph_count; index++) {
            CHECK(current->glyphs[index - 1].codepoint < current->glyphs[index].codepoint);
        }
    }
}

static void glyph_bitmaps_stay_inside_the_font_bitmap(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        const Font *current = ALL_FONTS[font];
        for (uint16_t index = 0; index < current->glyph_count; index++) {
            const Glyph *glyph = &current->glyphs[index];
            int packed_bytes = (glyph->width * glyph->height + 1) / 2;
            CHECK(glyph->bitmap_offset + packed_bytes <= current->bitmap_size);
        }
    }
}

static void every_font_covers_cyrillic(void)
{
    const uint32_t cyrillic[] = {0x0410, 0x042F, 0x0430, 0x044F, 0x0401, 0x0451,
                                 0x0404, 0x0454, 0x0406, 0x0456, 0x0407, 0x0457};
    for (size_t font = 0; font < FONT_COUNT; font++) {
        for (size_t index = 0; index < sizeof cyrillic / sizeof cyrillic[0]; index++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], cyrillic[index]);
            CHECK(glyph != NULL && glyph->codepoint == cyrillic[index]);
        }
    }
}

static void cjk_characters_fall_back_to_square_with_cjk_advance(void)
{
    const uint32_t cjk[] = {0x4E00, 0x9FFF, 0x3055, 0x30AB, 0xF900, 0xFF01};
    for (size_t font = 0; font < FONT_COUNT; font++) {
        uint8_t expected_advance = font == 0 ? 11 : 15;
        for (size_t index = 0; index < sizeof cjk / sizeof cjk[0]; index++) {
            const Glyph *glyph = font_find_glyph(ALL_FONTS[font], cjk[index]);
            CHECK(glyph != NULL);
            CHECK_EQUAL(FONT_CJK_FALLBACK_CODEPOINT, glyph->codepoint);
            CHECK_EQUAL(expected_advance, glyph->advance);
        }
    }
}

static void ideographic_space_has_cjk_advance_without_ink(void)
{
    for (size_t font = 0; font < FONT_COUNT; font++) {
        uint8_t expected_advance = font == 0 ? 11 : 15;
        const Glyph *glyph = font_find_glyph(ALL_FONTS[font], 0x3000);
        CHECK(glyph != NULL);
        CHECK_EQUAL(expected_advance, glyph->advance);
        CHECK_EQUAL(0, glyph->width);
        CHECK_EQUAL(0, glyph->height);
    }
}

static void mixed_text_measurement_and_clipping(void)
{
    const char *mixed = "Песня / \xE6\xAD\x8C / Song";
    int width = text_width(&FONT_INTER_REGULAR_16, mixed);
    CHECK(width > 0);
    LabelFit fit = label_fit(&FONT_INTER_REGULAR_16, mixed, width / 2);
    CHECK(fit.ellipsis);
    CHECK(fit.byte_count > 0 && fit.byte_count < strlen(mixed));
    int offset = marquee_offset(
        (MarqueeTiming){.speed_px_per_s = 20, .pause_ms = 1000, .gap_px = 30}, width, width / 2, 2000);
    CHECK(offset > 0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_font_covers_printable_ascii),
        TEST_CASE(every_font_covers_western_european_accents),
        TEST_CASE(every_font_covers_cyrillic),
        TEST_CASE(cjk_characters_fall_back_to_square_with_cjk_advance),
        TEST_CASE(ideographic_space_has_cjk_advance_without_ink),
        TEST_CASE(mixed_text_measurement_and_clipping),
        TEST_CASE(glyphs_are_sorted_by_codepoint),
        TEST_CASE(glyph_bitmaps_stay_inside_the_font_bitmap),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
