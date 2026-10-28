#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/eq.h"
#include "settings_store.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    SAVE_PRESET_ROW_SAVE,
    SAVE_PRESET_ROW_EDIT,
    SAVE_PRESET_ROW_CANCEL,
    SAVE_PRESET_ROW_COUNT
};

typedef struct SavePresetScreen {
    Screen screen;
    SettingsStore *store;
    const Theme *theme;
    char name_buffer[EQ_USER_PRESET_NAME_MAX + 1];
    int char_cursor;
    bool editing_name;
    int selected_row;
    uint32_t now_ms;
} SavePresetScreen;

Screen *save_preset_screen_init(SavePresetScreen *save_screen, SettingsStore *store,
                                const Theme *theme);
