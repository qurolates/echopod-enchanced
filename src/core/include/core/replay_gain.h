#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum ReplayGainMode {
    REPLAY_GAIN_OFF = 0,
    REPLAY_GAIN_TRACK = 1,
    REPLAY_GAIN_ALBUM = 2
} ReplayGainMode;

enum {
    REPLAY_GAIN_UNITY_Q16 = 65536,
    REPLAY_GAIN_MODE_COUNT = 3
};

typedef struct ReplayGainValues {
    bool has_track_gain;
    bool has_album_gain;
    bool has_track_peak;
    bool has_album_peak;
    int16_t track_gain_cb;
    int16_t album_gain_cb;
    uint32_t track_peak_q16;
    uint32_t album_peak_q16;
} ReplayGainValues;

bool replay_gain_parse_gain(const char *text, uint32_t length, int16_t *gain_cb);
bool replay_gain_parse_peak(const char *text, uint32_t length, uint32_t *peak_q16);
bool replay_gain_assign_field(ReplayGainValues *values, const char *key, uint32_t key_len,
                             const char *value, uint32_t val_len);
uint32_t replay_gain_calculate_multiplier(const ReplayGainValues *values, ReplayGainMode mode);
