#include <string.h>

#include "check.h"
#include "core/hash.h"
#include "settings/settings.h"

enum {
    VERSION_FIELD = 4,
    BRIGHTNESS_FIELD = 6,
    AUTO_OFF_FIELD = 14,
    EQ_PRESET_FIELD = 16,
    EQ_USER_COUNT_FIELD = 22,
    USER_PRESETS_BASE = 24,
    LEGACY_CHECKSUM_FIELD = 20,
    LEGACY_FILE_SIZE = 24,
    V3_CHECKSUM_FIELD = 28,
    V3_FILE_SIZE = 32,
    CHECKSUM_FIELD = 252,
    NEVER_OPTION = 6
};

static Settings custom_settings(void)
{
    Settings settings = settings_defaults();
    settings.brightness = 3;
    settings.screen_timeout = 4;
    settings.gain = GAIN_HIGH;
    settings.output = OUTPUT_BALANCED;
    settings.start_screen = START_FAVORITES;
    settings.shuffle = SHUFFLE_ALBUMS;
    settings.repeat = REPEAT_ONE;
    settings.volume = 42;
    settings.auto_off = 3;
    settings.eq_enabled = true;
    settings.eq_preset = 5;
    settings.eq_gains[0] = 3;
    settings.eq_gains[1] = 1;
    settings.eq_gains[2] = -1;
    settings.eq_gains[3] = 2;
    settings.eq_gains[4] = 2;
    return settings;
}

static void rewrite_checksum(uint8_t *bytes, size_t checksum_offset)
{
    uint32_t checksum = hash_update(HASH_START, bytes, checksum_offset);
    for (size_t index = 0; index < 4; index++) {
        bytes[checksum_offset + index] = (uint8_t)(checksum >> (8 * index));
    }
}

static void defaults_are_comfortable(void)
{
    Settings settings = settings_defaults();
    CHECK_EQUAL(7, settings.brightness);
    CHECK_EQUAL(30000, settings_screen_timeout_ms(&settings));
    CHECK_EQUAL(GAIN_LOW, settings.gain);
    CHECK_EQUAL(OUTPUT_AUTO, settings.output);
    CHECK_EQUAL(START_MAIN_MENU, settings.start_screen);
    CHECK_EQUAL(SHUFFLE_OFF, settings.shuffle);
    CHECK_EQUAL(REPEAT_OFF, settings.repeat);
    CHECK_EQUAL(12, settings.volume);
    CHECK_EQUAL(5u * 60000u, settings_auto_off_ms(&settings));
    CHECK(!settings.eq_enabled);
    CHECK_EQUAL(0, settings.eq_preset);
    CHECK_EQUAL(0, settings.eq_user_preset_count);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK_EQUAL(0, settings.eq_gains[band]);
    }
}

static void auto_off_options_are_stocks(void)
{
    CHECK_EQUAL(0, settings_auto_off_option_minutes(0));
    CHECK_EQUAL(1, settings_auto_off_option_minutes(1));
    CHECK_EQUAL(5, settings_auto_off_option_minutes(2));
    CHECK_EQUAL(10, settings_auto_off_option_minutes(AUTO_OFF_OPTION_COUNT - 1));
}

static void a_version_one_file_keeps_its_values_and_gets_the_default_auto_off(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 1;
    bytes[AUTO_OFF_FIELD] = 0;
    rewrite_checksum(bytes, LEGACY_CHECKSUM_FIELD);
    Settings decoded;
    CHECK(settings_decode(bytes, LEGACY_FILE_SIZE, &decoded));
    CHECK_EQUAL(42, decoded.volume);
    CHECK_EQUAL(settings_defaults().auto_off, decoded.auto_off);
    CHECK(!decoded.eq_enabled);
    CHECK_EQUAL(0, decoded.eq_preset);
    CHECK_EQUAL(0, decoded.eq_user_preset_count);
}

static void a_version_two_file_keeps_its_values_and_gets_default_eq(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 2;
    bytes[AUTO_OFF_FIELD] = 2;
    rewrite_checksum(bytes, LEGACY_CHECKSUM_FIELD);
    Settings decoded;
    CHECK(settings_decode(bytes, LEGACY_FILE_SIZE, &decoded));
    CHECK_EQUAL(42, decoded.volume);
    CHECK_EQUAL(2, decoded.auto_off);
    CHECK(!decoded.eq_enabled);
    CHECK_EQUAL(0, decoded.eq_preset);
    CHECK_EQUAL(0, decoded.eq_user_preset_count);
}

static void a_version_three_file_decodes_eq_settings(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 3;
    rewrite_checksum(bytes, V3_CHECKSUM_FIELD);
    Settings decoded;
    CHECK(settings_decode(bytes, V3_FILE_SIZE, &decoded));
    CHECK_EQUAL(42, decoded.volume);
    CHECK(decoded.eq_enabled);
    CHECK_EQUAL(5, decoded.eq_preset);
    CHECK_EQUAL(3, decoded.eq_gains[0]);
    CHECK_EQUAL(0, decoded.eq_user_preset_count);
}

static void timeout_options_end_with_never(void)
{
    CHECK_EQUAL(10, settings_timeout_option_seconds(0));
    CHECK_EQUAL(300, settings_timeout_option_seconds(5));
    CHECK_EQUAL(0, settings_timeout_option_seconds(NEVER_OPTION));
    Settings settings = settings_defaults();
    settings.screen_timeout = NEVER_OPTION;
    CHECK_EQUAL(0, settings_screen_timeout_ms(&settings));
}

static void sleep_options_start_with_off(void)
{
    CHECK_EQUAL(0, settings_sleep_option_minutes(0));
    CHECK_EQUAL(15, settings_sleep_option_minutes(1));
    CHECK_EQUAL(120, settings_sleep_option_minutes(SLEEP_TIMER_OPTION_COUNT - 1));
}

static void settings_survive_a_round_trip(void)
{
    Settings original = custom_settings();
    int8_t user_gains[EQ_BAND_COUNT] = {4, 2, 0, -2, -4};
    CHECK(settings_save_user_preset(&original, "Bass Plus", user_gains));
    uint8_t bytes[SETTINGS_FILE_SIZE];
    Settings decoded;
    settings_encode(&original, bytes);
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(original.brightness, decoded.brightness);
    CHECK_EQUAL(original.screen_timeout, decoded.screen_timeout);
    CHECK_EQUAL(original.gain, decoded.gain);
    CHECK_EQUAL(original.output, decoded.output);
    CHECK_EQUAL(original.start_screen, decoded.start_screen);
    CHECK_EQUAL(original.shuffle, decoded.shuffle);
    CHECK_EQUAL(original.repeat, decoded.repeat);
    CHECK_EQUAL(original.volume, decoded.volume);
    CHECK_EQUAL(original.auto_off, decoded.auto_off);
    CHECK_EQUAL(original.eq_enabled, decoded.eq_enabled);
    CHECK_EQUAL(original.eq_preset, decoded.eq_preset);
    CHECK_EQUAL(1, decoded.eq_user_preset_count);
    CHECK(strcmp("Bass Plus", decoded.eq_user_presets[0].name) == 0);
    CHECK_EQUAL(4, decoded.eq_user_presets[0].gains[0]);
    CHECK_EQUAL(-4, decoded.eq_user_presets[0].gains[4]);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK_EQUAL(original.eq_gains[band], decoded.eq_gains[band]);
    }
}

static void a_corrupted_file_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[BRIGHTNESS_FIELD] ^= 0x02;
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    CHECK(!settings_decode(bytes, SETTINGS_FILE_SIZE - 1, &settings));
}

static void out_of_range_values_are_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[BRIGHTNESS_FIELD] = SETTINGS_BRIGHTNESS_MAX + 1;
    rewrite_checksum(bytes, CHECKSUM_FIELD);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));

    settings_encode(&settings, bytes);
    bytes[EQ_PRESET_FIELD] = 200;
    rewrite_checksum(bytes, CHECKSUM_FIELD);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void user_preset_lifecycle_and_capacity(void)
{
    Settings settings = settings_defaults();
    int8_t gains[EQ_BAND_COUNT] = {1, 2, 3, 4, 5};
    CHECK(!settings_save_user_preset(&settings, "", gains));
    CHECK(!settings_save_user_preset(&settings, NULL, gains));

    for (int index = 0; index < EQ_USER_PRESET_CAPACITY; index++) {
        char name[16];
        name[0] = 'P';
        name[1] = (char)('0' + index);
        name[2] = '\0';
        CHECK(settings_save_user_preset(&settings, name, gains));
    }
    CHECK_EQUAL(EQ_USER_PRESET_CAPACITY, settings.eq_user_preset_count);
    CHECK(!settings_save_user_preset(&settings, "Overflow", gains));

    int8_t updated_gains[EQ_BAND_COUNT] = {6, 6, 6, 6, 6};
    CHECK(settings_save_user_preset(&settings, "P0", updated_gains));
    CHECK_EQUAL(EQ_USER_PRESET_CAPACITY, settings.eq_user_preset_count);
    CHECK_EQUAL(6, settings.eq_user_presets[0].gains[0]);

    uint8_t active_id = settings.eq_user_presets[0].id;
    settings.eq_preset = active_id;
    CHECK(settings_delete_user_preset(&settings, active_id));
    CHECK_EQUAL(EQ_USER_PRESET_CAPACITY - 1, settings.eq_user_preset_count);
    CHECK_EQUAL(EQ_PRESET_CUSTOM, settings.eq_preset);

    CHECK(!settings_delete_user_preset(&settings, 99));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(defaults_are_comfortable),
        TEST_CASE(timeout_options_end_with_never),
        TEST_CASE(sleep_options_start_with_off),
        TEST_CASE(settings_survive_a_round_trip),
        TEST_CASE(a_corrupted_file_is_rejected),
        TEST_CASE(out_of_range_values_are_rejected),
        TEST_CASE(auto_off_options_are_stocks),
        TEST_CASE(a_version_one_file_keeps_its_values_and_gets_the_default_auto_off),
        TEST_CASE(a_version_two_file_keeps_its_values_and_gets_default_eq),
        TEST_CASE(a_version_three_file_decodes_eq_settings),
        TEST_CASE(user_preset_lifecycle_and_capacity),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
