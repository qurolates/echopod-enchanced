#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/eq.h"
#include "settings_store.h"
#include "ui/screen.h"
#include "ui/theme.h"

typedef struct CustomEqScreen {
    Screen screen;
    SettingsStore *store;
    const Theme *theme;
    int selected_band;
    bool adjusting;
    uint32_t now_ms;
} CustomEqScreen;

Screen *custom_eq_screen_init(CustomEqScreen *custom_screen, SettingsStore *store,
                              const Theme *theme);
