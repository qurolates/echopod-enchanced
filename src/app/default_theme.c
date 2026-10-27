#include "default_theme.h"

#include "fonts/fonts.h"
#include "hal/display.h"

const Theme DEFAULT_THEME = {
    .colors =
        {
            .background = COLOR_RGB(230, 232, 235),
            .text = COLOR_RGB(35, 38, 43),
            .text_secondary = COLOR_RGB(92, 97, 105),
            .text_tertiary = COLOR_RGB(135, 139, 146),
            .selection_background = COLOR_RGB(35, 105, 205),
            .selection_top = COLOR_RGB(93, 157, 238),
            .selection_shine = COLOR_RGB(160, 204, 255),
            .selection_text = COLOR_RGB(255, 255, 255),
            .header_background = COLOR_RGB(195, 200, 207),
            .header_top = COLOR_RGB(215, 220, 225),
            .header_text = COLOR_RGB(35, 38, 43),
            .divider = COLOR_RGB(190, 194, 201),
            .scrollbar_thumb = COLOR_RGB(133, 139, 148),
            .danger = COLOR_RGB(210, 54, 48),
            .success = COLOR_RGB(48, 145, 75),
            .chevron = COLOR_RGB(105, 111, 120),
        },
    .fonts =
        {
            .title = &FONT_INTER_SEMIBOLD_16,
            .body = &FONT_INTER_REGULAR_16,
            .small = &FONT_INTER_REGULAR_12,
        },
    .metrics =
        {
            .screen_width = HAL_DISPLAY_WIDTH,
            .screen_height = HAL_DISPLAY_HEIGHT,
            .header_height = 24,
            .header_inset = 68,
            .row_height = 24,
            .padding = 10,
            .scrollbar_width = 3,
            .scrollbar_inset = 2,
            .detail_gap = 8,
            .status_icon_size = 10,
        },
    .marquee = {.pause_ms = 1200, .speed_px_per_s = 40, .gap_px = 32},
};
