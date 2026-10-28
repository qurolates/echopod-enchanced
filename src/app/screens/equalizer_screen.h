#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/eq.h"
#include "screens/custom_eq_screen.h"
#include "screens/save_preset_screen.h"
#include "settings_store.h"
#include "ui/choice_screen.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    EQ_ROW_ENABLED,
    EQ_ROW_PRESET,
    EQ_ROW_CUSTOM_EQ,
    EQ_ROW_RESET,
    EQ_ROW_SAVE_PRESET,
    EQ_DEFAULT_ROW_COUNT = 4
};

typedef struct EqualizerScreen {
    Screen screen;
    SettingsStore *store;
    const Theme *theme;
    ListModel model;
    ListView list;

    Screen preset_screen;
    ListModel preset_model;
    ListView preset_list;

    CustomEqScreen custom_eq;
    SavePresetScreen save_preset;
    ChoiceScreen reset_choice;
    ChoiceScreen delete_choice;

    uint8_t pending_delete_id;
    uint32_t now_ms;
} EqualizerScreen;

Screen *equalizer_screen_init(EqualizerScreen *eq_screen, SettingsStore *store,
                              const Theme *theme);
