#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/eq.h"
#include "settings_store.h"
#include "ui/choice_screen.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

typedef struct EqualizerScreen {
    Screen screen;
    SettingsStore *store;
    const Theme *theme;
    ListModel model;
    ListView list;
    ChoiceScreen preset_choice;
    bool adjusting;
    uint32_t now_ms;
    char value_buffers[EQ_BAND_COUNT][24];
} EqualizerScreen;

Screen *equalizer_screen_init(EqualizerScreen *eq_screen, SettingsStore *store,
                              const Theme *theme);
