#include <string.h>

#include "check.h"
#include "core/eq.h"

static void disabled_eq_leaves_pcm_unchanged(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, false);
    const int8_t gains[EQ_BAND_COUNT] = {6, 6, 6, 6, 6};
    eq_set_gains(&eq, gains);
    int32_t left = 123456;
    int32_t right = -654321;
    eq_process_frame(&eq, &left, &right);
    CHECK(left == 123456);
    CHECK(right == -654321);
}

static void flat_eq_leaves_pcm_unchanged(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    CHECK(eq_is_flat(&eq));
    int32_t left = 500000;
    int32_t right = -500000;
    eq_process_frame(&eq, &left, &right);
    CHECK(left == 500000);
    CHECK(right == -500000);
}

static void zero_signal_produces_zero_output(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    const int8_t gains[EQ_BAND_COUNT] = {12, 12, 12, 12, 12};
    eq_set_gains(&eq, gains);
    for (int frame = 0; frame < 100; frame++) {
        int32_t left = 0;
        int32_t right = 0;
        eq_process_frame(&eq, &left, &right);
        CHECK(left == 0);
        CHECK(right == 0);
    }
}

static void extreme_pcm_values_do_not_overflow_or_wrap(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    const int8_t boost[EQ_BAND_COUNT] = {12, 12, 12, 12, 12};
    eq_set_gains(&eq, boost);
    for (int frame = 0; frame < 500; frame++) {
        int32_t left = (frame % 2 == 0) ? 8388607 : -8388608;
        int32_t right = (frame % 2 == 0) ? -8388608 : 8388607;
        eq_process_frame(&eq, &left, &right);
        CHECK(left <= 8388607);
        CHECK(left >= -8388608);
        CHECK(right <= 8388607);
        CHECK(right >= -8388608);
    }
}

static void attenuation_reduces_amplitude(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    const int8_t cut[EQ_BAND_COUNT] = {-12, -12, -12, -12, -12};
    eq_set_gains(&eq, cut);
    int32_t left = 4000000;
    int32_t right = 4000000;
    for (int frame = 0; frame < 200; frame++) {
        left = 4000000;
        right = 4000000;
        eq_process_frame(&eq, &left, &right);
    }
    CHECK(left > 0 && left < 4000000);
    CHECK(right > 0 && right < 4000000);
}

static void stereo_channels_are_processed_independently(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    const int8_t gains[EQ_BAND_COUNT] = {6, -6, 3, -3, 0};
    eq_set_gains(&eq, gains);
    int32_t left = 1000000;
    int32_t right = 0;
    eq_process_frame(&eq, &left, &right);
    CHECK(left != 1000000);
    CHECK(right == 0);
}

static void all_presets_are_valid_and_non_null(void)
{
    for (int id = 0; id < EQ_PRESET_COUNT; id++) {
        const EqPreset *preset = eq_preset(id);
        CHECK(preset != NULL);
        CHECK(preset->name != NULL);
        for (int band = 0; band < EQ_BAND_COUNT; band++) {
            CHECK(preset->gains[band] >= EQ_GAIN_MIN);
            CHECK(preset->gains[band] <= EQ_GAIN_MAX);
        }
    }
    const EqPreset *flat = eq_preset(0);
    CHECK(strcmp(flat->name, "Flat") == 0);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK(flat->gains[band] == 0);
    }
    const EqPreset *acoustic = eq_preset(1);
    CHECK(strcmp(acoustic->name, "Acoustic") == 0);
    CHECK(acoustic->gains[0] == 1);
    CHECK(acoustic->gains[4] == 2);
    const EqPreset *custom = eq_preset(EQ_PRESET_CUSTOM);
    CHECK(strcmp(custom->name, "Custom") == 0);
}

static void reset_clears_filter_state(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    const int8_t gains[EQ_BAND_COUNT] = {6, 6, 6, 6, 6};
    eq_set_gains(&eq, gains);
    int32_t left = 2000000;
    int32_t right = 2000000;
    eq_process_frame(&eq, &left, &right);
    CHECK(eq.left.s1[0] != 0 || eq.left.s2[0] != 0);
    eq_reset(&eq);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK(eq.left.s1[band] == 0);
        CHECK(eq.left.s2[band] == 0);
        CHECK(eq.right.s1[band] == 0);
        CHECK(eq.right.s2[band] == 0);
    }
}

static void sample_rates_switch_cleanly(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    const int8_t gains[EQ_BAND_COUNT] = {3, 2, 1, 0, -1};
    eq_set_gains(&eq, gains);
    static const uint32_t RATES[] = {44100, 48000, 88200, 96000, 176400, 192000};
    for (size_t index = 0; index < sizeof RATES / sizeof RATES[0]; index++) {
        eq_set_sample_rate(&eq, RATES[index]);
        int32_t left = 500000;
        int32_t right = -500000;
        eq_process_frame(&eq, &left, &right);
        CHECK(left <= 8388607 && left >= -8388608);
        CHECK(right <= 8388607 && right >= -8388608);
    }
}

static void gain_stepping_converges_to_target(void)
{
    Eq eq;
    eq_init(&eq);
    eq_set_enabled(&eq, true);
    eq_set_band_gain(&eq, EQ_BAND_100_HZ, 5);
    eq.active_gains[EQ_BAND_100_HZ] = 0;
    for (int step = 0; step < 5; step++) {
        eq_step_gains(&eq);
    }
    CHECK(eq.active_gains[EQ_BAND_100_HZ] == 5);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(disabled_eq_leaves_pcm_unchanged),
        TEST_CASE(flat_eq_leaves_pcm_unchanged),
        TEST_CASE(zero_signal_produces_zero_output),
        TEST_CASE(extreme_pcm_values_do_not_overflow_or_wrap),
        TEST_CASE(attenuation_reduces_amplitude),
        TEST_CASE(stereo_channels_are_processed_independently),
        TEST_CASE(all_presets_are_valid_and_non_null),
        TEST_CASE(reset_clears_filter_state),
        TEST_CASE(sample_rates_switch_cleanly),
        TEST_CASE(gain_stepping_converges_to_target),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
