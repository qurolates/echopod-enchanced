#include "core/eq.h"

#include <stddef.h>

enum {
    RATE_COUNT = 6,
    SAMPLE_MAX = 8388607,
    SAMPLE_MIN = -8388608,
    GAIN_OFFSET = 12,
    Q30_ONE = 1 << 30,
    Q28_SHIFT = 28,
    Q24_SHIFT = 24,
    ROUND_HALF_Q28 = 1 << 27
};

static const uint32_t RATES[RATE_COUNT] = {44100, 48000, 88200, 96000, 176400, 192000};

static const int32_t COS_Q30[RATE_COUNT][EQ_BAND_COUNT] = {
    {1073632844, 1072761141, 1062862106, 977142760, 156250068},
    {1073649834, 1072914008, 1064555814, 992008094, 277904834},
    {1073714579, 1073496625, 1071018441, 1049314194, 812617295},
    {1073718826, 1073534850, 1071442860, 1053110176, 851856663},
    {1073735013, 1073680523, 1073060762, 1067617450, 1006345537},
    {1073736075, 1073690079, 1073166929, 1068571464, 1016758484},
};

static const int32_t ALPHA_Q30[RATE_COUNT][EQ_BAND_COUNT] = {
    {6953505, 20854869, 69302381, 202317937, 482869227},
    {6388566, 19161321, 63705196, 186774185, 471434072},
    {3476841, 10429816, 34739301, 103513910, 319016644},
    {3194352, 9582508, 31920943, 95216654, 297114821},
    {1738431, 5215206, 17380675, 52053858, 170190804},
    {1597184, 4791485, 15969022, 47838683, 156883181},
};

static const int32_t A_Q24[EQ_GAIN_MAX - EQ_GAIN_MIN + 1] = {
    8408526,  8906763,  9434522,  9993552,  10585708, 11212950, 11877359, 12581137, 13326616,
    14116268, 14952709, 15838713, 16777216, 17771329, 18824346, 19939759, 21121264, 22372777,
    23698447, 25102669, 26590095, 28165658, 29834578, 31602388, 33474947};

static const EqPreset PRESETS[EQ_PRESET_COUNT] = {
    {"Flat", {0, 0, 0, 0, 0}},
    {"Acoustic", {1, 0, 0, 1, 2}},
    {"Bass Boost", {4, 2, 0, 0, 0}},
    {"Deep Bass", {5, 2, -1, 0, -1}},
    {"Pop", {2, 1, 0, 2, 1}},
    {"Rock", {3, 1, -1, 2, 2}},
    {"Alternative Rock", {2, 0, 1, 2, 1}},
    {"Metal", {3, 1, -1, 2, 1}},
    {"Punk", {2, 1, 0, 2, 1}},
    {"Hip-Hop", {4, 2, 0, 1, 1}},
    {"Electronic / EDM", {3, 1, 0, 2, 2}},
    {"R&B", {3, 1, 1, 2, 1}},
    {"Jazz", {1, 0, 1, 2, 1}},
    {"Classical", {-1, -1, 0, 1, 2}},
    {"Vocal", {1, 0, 2, 3, 1}},
    {"Folk / Country", {1, 0, 1, 2, 1}},
    {"Ambient", {1, -1, 0, 1, 2}},
};

static const EqPreset CUSTOM_PRESET = {"Custom", {0, 0, 0, 0, 0}};

static const char *const BAND_NAMES[EQ_BAND_COUNT] = {"100 Hz", "300 Hz", "1 kHz", "3 kHz",
                                                      "10 kHz"};

const EqPreset *eq_preset(int id)
{
    if (id >= 0 && id < EQ_PRESET_COUNT) {
        return &PRESETS[id];
    }
    return &CUSTOM_PRESET;
}

const char *eq_band_name(int band)
{
    if (band >= 0 && band < EQ_BAND_COUNT) {
        return BAND_NAMES[band];
    }
    return "";
}

static int rate_index(uint32_t sample_rate)
{
    for (int index = 0; index < RATE_COUNT; index++) {
        if (RATES[index] == sample_rate) {
            return index;
        }
    }
    return 0;
}

static int8_t clamp_gain(int8_t gain)
{
    if (gain < EQ_GAIN_MIN) {
        return EQ_GAIN_MIN;
    }
    if (gain > EQ_GAIN_MAX) {
        return EQ_GAIN_MAX;
    }
    return gain;
}

static void compute_band_coeffs(EqBiquadCoeffs *c, int rate_idx, int band, int8_t gain)
{
    if (gain == 0) {
        *c = (EqBiquadCoeffs){.b0 = 1 << Q28_SHIFT, .b1 = 0, .b2 = 0, .a1 = 0, .a2 = 0};
        return;
    }
    int32_t cos_q30 = COS_Q30[rate_idx][band];
    int32_t alpha_q30 = ALPHA_Q30[rate_idx][band];
    int32_t a_q24 = A_Q24[gain + GAIN_OFFSET];
    int32_t inv_a_q24 = A_Q24[-gain + GAIN_OFFSET];

    int32_t alpha_a = (int32_t)(((int64_t)alpha_q30 * a_q24) >> Q24_SHIFT);
    int32_t alpha_inv_a = (int32_t)(((int64_t)alpha_q30 * inv_a_q24) >> Q24_SHIFT);

    int64_t b0 = (int64_t)Q30_ONE + alpha_a;
    int64_t b1 = -((int64_t)cos_q30 << 1);
    int64_t b2 = (int64_t)Q30_ONE - alpha_a;
    int64_t a0 = (int64_t)Q30_ONE + alpha_inv_a;
    int64_t a1 = b1;
    int64_t a2 = (int64_t)Q30_ONE - alpha_inv_a;

    int64_t scale = (int64_t)1 << Q28_SHIFT;
    c->b0 = (int32_t)((b0 * scale) / a0);
    c->b1 = (int32_t)((b1 * scale) / a0);
    c->b2 = (int32_t)((b2 * scale) / a0);
    c->a1 = (int32_t)((a1 * scale) / a0);
    c->a2 = (int32_t)((a2 * scale) / a0);
}

static void recompute_coeffs(Eq *eq)
{
    int rate_idx = rate_index(eq->sample_rate);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        compute_band_coeffs(&eq->coeffs[band], rate_idx, band, eq->active_gains[band]);
    }
}

void eq_init(Eq *eq)
{
    *eq = (Eq){.sample_rate = 44100};
    recompute_coeffs(eq);
}

void eq_set_enabled(Eq *eq, bool enabled)
{
    eq->enabled = enabled;
}

void eq_set_sample_rate(Eq *eq, uint32_t sample_rate)
{
    if (eq->sample_rate == sample_rate) {
        return;
    }
    eq->sample_rate = sample_rate;
    recompute_coeffs(eq);
}

void eq_set_gains(Eq *eq, const int8_t *gains)
{
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        eq->target_gains[band] = clamp_gain(gains[band]);
        eq->active_gains[band] = eq->target_gains[band];
    }
    recompute_coeffs(eq);
}

void eq_set_band_gain(Eq *eq, int band, int8_t gain)
{
    if (band < 0 || band >= EQ_BAND_COUNT) {
        return;
    }
    eq->target_gains[band] = clamp_gain(gain);
    eq->active_gains[band] = eq->target_gains[band];
    recompute_coeffs(eq);
}

void eq_reset(Eq *eq)
{
    eq->left = (EqChannelState){0};
    eq->right = (EqChannelState){0};
}

bool eq_is_flat(const Eq *eq)
{
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        if (eq->active_gains[band] != 0) {
            return false;
        }
    }
    return true;
}

void eq_step_gains(Eq *eq)
{
    bool changed = false;
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        if (eq->active_gains[band] < eq->target_gains[band]) {
            eq->active_gains[band]++;
            changed = true;
        } else if (eq->active_gains[band] > eq->target_gains[band]) {
            eq->active_gains[band]--;
            changed = true;
        }
    }
    if (changed) {
        recompute_coeffs(eq);
    }
}

static int32_t process_biquad(int64_t *s1, int64_t *s2, const EqBiquadCoeffs *c, int32_t x)
{
    int64_t y_q28 = (int64_t)c->b0 * x + *s1;
    int32_t y = (int32_t)((y_q28 + ROUND_HALF_Q28) >> Q28_SHIFT);
    if (y > SAMPLE_MAX) {
        y = SAMPLE_MAX;
    } else if (y < SAMPLE_MIN) {
        y = SAMPLE_MIN;
    }
    *s1 = (int64_t)c->b1 * x - (int64_t)c->a1 * y + *s2;
    *s2 = (int64_t)c->b2 * x - (int64_t)c->a2 * y;
    return y;
}

void eq_process_frame(Eq *eq, int32_t *left, int32_t *right)
{
    if (!eq->enabled || eq_is_flat(eq)) {
        return;
    }
    int32_t l = *left;
    int32_t r = *right;
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        if (eq->active_gains[band] == 0) {
            continue;
        }
        const EqBiquadCoeffs *c = &eq->coeffs[band];
        l = process_biquad(&eq->left.s1[band], &eq->left.s2[band], c, l);
        r = process_biquad(&eq->right.s1[band], &eq->right.s2[band], c, r);
    }
    *left = l;
    *right = r;
}
