#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/eq.h"
#include "core/replay_gain.h"
#include "playback/play_queue.h"

typedef enum GainLevel {
    GAIN_LOW,
    GAIN_HIGH
} GainLevel;

typedef enum OutputJack {
    OUTPUT_AUTO,
    OUTPUT_SINGLE_ENDED,
    OUTPUT_BALANCED
} OutputJack;

typedef enum StartScreen {
    START_MAIN_MENU,
    START_NOW_PLAYING,
    START_ALBUMS,
    START_ARTISTS,
    START_FAVORITES,
    START_PLAYLISTS,
    START_COVER_FLOW
} StartScreen;

enum {
    GAIN_LEVEL_COUNT = GAIN_HIGH + 1,
    OUTPUT_JACK_COUNT = OUTPUT_BALANCED + 1,
    START_SCREEN_COUNT = START_COVER_FLOW + 1,
    SETTINGS_BRIGHTNESS_MIN = 1,
    SETTINGS_BRIGHTNESS_MAX = 10,
    SETTINGS_VOLUME_MAX = 50,
    SCREEN_TIMEOUT_OPTION_COUNT = 7,
    SLEEP_TIMER_OPTION_COUNT = 7,
    AUTO_OFF_OPTION_COUNT = 4,
    EQ_USER_PRESET_NAME_MAX = 15,
    EQ_USER_PRESET_CAPACITY = 8,
    SETTINGS_FILE_SIZE = 256,
    SETTINGS_V3_FILE_SIZE = 32,
    SETTINGS_LEGACY_FILE_SIZE = 24
};

typedef struct EqUserPreset {
    uint8_t id;
    char name[EQ_USER_PRESET_NAME_MAX + 1];
    int8_t gains[EQ_BAND_COUNT];
} EqUserPreset;

typedef struct Settings {
    uint8_t brightness;
    uint8_t screen_timeout;
    GainLevel gain;
    OutputJack output;
    StartScreen start_screen;
    ShuffleMode shuffle;
    RepeatMode repeat;
    uint8_t volume;
    uint8_t auto_off;
    bool eq_enabled;
    uint8_t eq_preset;
    int8_t eq_gains[EQ_BAND_COUNT];
    uint8_t eq_user_preset_count;
    EqUserPreset eq_user_presets[EQ_USER_PRESET_CAPACITY];
    ReplayGainMode replay_gain_mode;
} Settings;

Settings settings_defaults(void);
const char *settings_preset_name(const Settings *settings, uint8_t preset_id);
int settings_find_user_preset_by_id(const Settings *settings, uint8_t id);
int settings_find_user_preset_by_name(const Settings *settings, const char *name);
bool settings_save_user_preset(Settings *settings, const char *name, const int8_t *gains);
bool settings_delete_user_preset(Settings *settings, uint8_t id);
uint32_t settings_timeout_option_seconds(int option);
uint32_t settings_sleep_option_minutes(int option);
uint32_t settings_screen_timeout_ms(const Settings *settings);
uint32_t settings_auto_off_option_minutes(int option);
uint32_t settings_auto_off_ms(const Settings *settings);
void settings_encode(const Settings *settings, uint8_t *bytes);
bool settings_decode(const uint8_t *bytes, uint32_t length, Settings *settings);
