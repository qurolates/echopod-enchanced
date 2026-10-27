#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    EQ_BAND_COUNT = 5,
    EQ_GAIN_MIN = -12,
    EQ_GAIN_MAX = 12,
    EQ_PRESET_COUNT = 17,
    EQ_PRESET_CUSTOM = 17,
    EQ_BAND_100_HZ = 0,
    EQ_BAND_300_HZ = 1,
    EQ_BAND_1_KHZ = 2,
    EQ_BAND_3_KHZ = 3,
    EQ_BAND_10_KHZ = 4
};

typedef struct EqPreset {
    const char *name;
    int8_t gains[EQ_BAND_COUNT];
} EqPreset;

typedef struct EqBiquadCoeffs {
    int32_t b0;
    int32_t b1;
    int32_t b2;
    int32_t a1;
    int32_t a2;
} EqBiquadCoeffs;

typedef struct EqChannelState {
    int64_t s1[EQ_BAND_COUNT];
    int64_t s2[EQ_BAND_COUNT];
} EqChannelState;

typedef struct Eq {
    bool enabled;
    int8_t active_gains[EQ_BAND_COUNT];
    int8_t target_gains[EQ_BAND_COUNT];
    uint32_t sample_rate;
    EqBiquadCoeffs coeffs[EQ_BAND_COUNT];
    EqChannelState left;
    EqChannelState right;
} Eq;

const EqPreset *eq_preset(int id);
const char *eq_band_name(int band);
void eq_init(Eq *eq);
void eq_set_enabled(Eq *eq, bool enabled);
void eq_set_sample_rate(Eq *eq, uint32_t sample_rate);
void eq_set_gains(Eq *eq, const int8_t *gains);
void eq_set_band_gain(Eq *eq, int band, int8_t gain);
void eq_reset(Eq *eq);
bool eq_is_flat(const Eq *eq);
void eq_step_gains(Eq *eq);
void eq_process_frame(Eq *eq, int32_t *left, int32_t *right);
