#include <string.h>

#include "check.h"
#include "core/replay_gain.h"

static void parse_gain_valid_strings(void)
{
    int16_t gain = 0;
    const char *s1 = "-6.50 dB";
    CHECK(replay_gain_parse_gain(s1, (uint32_t)strlen(s1), &gain));
    CHECK_EQUAL(-650, gain);

    const char *s2 = "+2.3 dB";
    CHECK(replay_gain_parse_gain(s2, (uint32_t)strlen(s2), &gain));
    CHECK_EQUAL(230, gain);

    const char *s3 = "-8.2";
    CHECK(replay_gain_parse_gain(s3, (uint32_t)strlen(s3), &gain));
    CHECK_EQUAL(-820, gain);

    const char *s4 = "0 dB";
    CHECK(replay_gain_parse_gain(s4, (uint32_t)strlen(s4), &gain));
    CHECK_EQUAL(0, gain);

    const char *s5 = "   3.456 dB   ";
    CHECK(replay_gain_parse_gain(s5, (uint32_t)strlen(s5), &gain));
    CHECK_EQUAL(346, gain);
}

static void parse_gain_invalid_strings(void)
{
    int16_t gain = 0;
    const char *nan_str = "NaN";
    CHECK(!replay_gain_parse_gain(nan_str, (uint32_t)strlen(nan_str), &gain));

    const char *inf_str = "Inf";
    CHECK(!replay_gain_parse_gain(inf_str, (uint32_t)strlen(inf_str), &gain));

    const char *double_minus = "--6.0";
    CHECK(!replay_gain_parse_gain(double_minus, (uint32_t)strlen(double_minus), &gain));

    const char *letters = "xyz dB";
    CHECK(!replay_gain_parse_gain(letters, (uint32_t)strlen(letters), &gain));

    CHECK(!replay_gain_parse_gain(NULL, 0, &gain));
    CHECK(!replay_gain_parse_gain("", 0, &gain));
}

static void parse_peak_valid_strings(void)
{
    uint32_t peak = 0;
    const char *s1 = "1.0";
    CHECK(replay_gain_parse_peak(s1, (uint32_t)strlen(s1), &peak));
    CHECK_EQUAL(65536, (int)peak);

    const char *s2 = "0.5";
    CHECK(replay_gain_parse_peak(s2, (uint32_t)strlen(s2), &peak));
    CHECK_EQUAL(32768, (int)peak);

    const char *s3 = "0.988281";
    CHECK(replay_gain_parse_peak(s3, (uint32_t)strlen(s3), &peak));
    CHECK(peak > 64700 && peak < 64800);
}

static void parse_peak_invalid_strings(void)
{
    uint32_t peak = 0;
    const char *neg = "-0.5";
    CHECK(!replay_gain_parse_peak(neg, (uint32_t)strlen(neg), &peak));

    const char *zero = "0.0";
    CHECK(!replay_gain_parse_peak(zero, (uint32_t)strlen(zero), &peak));

    const char *letters = "bad";
    CHECK(!replay_gain_parse_peak(letters, (uint32_t)strlen(letters), &peak));

    CHECK(!replay_gain_parse_peak(NULL, 0, &peak));
    CHECK(!replay_gain_parse_peak("", 0, &peak));
}

static void assign_fields_case_insensitive(void)
{
    ReplayGainValues v = {0};
    const char *k1 = "REPLAYGAIN_TRACK_GAIN";
    const char *v1 = "-4.50 dB";
    CHECK(replay_gain_assign_field(&v, k1, (uint32_t)strlen(k1), v1, (uint32_t)strlen(v1)));
    CHECK(v.has_track_gain);
    CHECK_EQUAL(-450, v.track_gain_cb);

    const char *k2 = "replaygain_album_gain";
    const char *v2 = "+1.20 dB";
    CHECK(replay_gain_assign_field(&v, k2, (uint32_t)strlen(k2), v2, (uint32_t)strlen(v2)));
    CHECK(v.has_album_gain);
    CHECK_EQUAL(120, v.album_gain_cb);

    const char *k3 = "ReplayGain_Track_Peak";
    const char *v3 = "0.95";
    CHECK(replay_gain_assign_field(&v, k3, (uint32_t)strlen(k3), v3, (uint32_t)strlen(v3)));
    CHECK(v.has_track_peak);
    CHECK(v.track_peak_q16 > 62000);

    const char *k4 = "replaygain_album_peak";
    const char *v4 = "1.000";
    CHECK(replay_gain_assign_field(&v, k4, (uint32_t)strlen(k4), v4, (uint32_t)strlen(v4)));
    CHECK(v.has_album_peak);
    CHECK_EQUAL(65536, (int)v.album_peak_q16);
}

static void multiplier_respects_modes_and_neutral(void)
{
    ReplayGainValues v = {
        .has_track_gain = true,
        .track_gain_cb = -602,
        .has_album_gain = true,
        .album_gain_cb = 602,
    };

    uint32_t off_mult = replay_gain_calculate_multiplier(&v, REPLAY_GAIN_OFF);
    CHECK_EQUAL(65536, (int)off_mult);

    uint32_t track_mult = replay_gain_calculate_multiplier(&v, REPLAY_GAIN_TRACK);
    CHECK(track_mult >= 32750 && track_mult <= 32780);

    uint32_t album_mult = replay_gain_calculate_multiplier(&v, REPLAY_GAIN_ALBUM);
    CHECK(album_mult >= 131000 && album_mult <= 131150);

    ReplayGainValues no_album = {
        .has_track_gain = true,
        .track_gain_cb = -600,
        .has_album_gain = false,
    };
    CHECK_EQUAL(65536, (int)replay_gain_calculate_multiplier(&no_album, REPLAY_GAIN_ALBUM));

    ReplayGainValues no_track = {
        .has_track_gain = false,
        .has_album_gain = true,
        .album_gain_cb = -600,
    };
    CHECK_EQUAL(65536, (int)replay_gain_calculate_multiplier(&no_track, REPLAY_GAIN_TRACK));
}

static void anti_clipping_limits_multiplier_when_peak_present(void)
{
    ReplayGainValues v = {
        .has_track_gain = true,
        .track_gain_cb = 602,
        .has_track_peak = true,
        .track_peak_q16 = 65536,
    };
    uint32_t mult = replay_gain_calculate_multiplier(&v, REPLAY_GAIN_TRACK);
    CHECK(mult <= 65536);

    ReplayGainValues v2 = {
        .has_track_gain = true,
        .track_gain_cb = 602,
        .has_track_peak = true,
        .track_peak_q16 = 32768,
    };
    uint32_t mult2 = replay_gain_calculate_multiplier(&v2, REPLAY_GAIN_TRACK);
    CHECK(mult2 >= 131000 && mult2 <= 131150);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(parse_gain_valid_strings),
        TEST_CASE(parse_gain_invalid_strings),
        TEST_CASE(parse_peak_valid_strings),
        TEST_CASE(parse_peak_invalid_strings),
        TEST_CASE(assign_fields_case_insensitive),
        TEST_CASE(multiplier_respects_modes_and_neutral),
        TEST_CASE(anti_clipping_limits_multiplier_when_peak_present),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
