#include <string.h>

#include "app/app.h"
#include "app_internal.h"
#include "check.h"
#include "core/power_save.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "hal/audio.h"
#include "hal/display.h"
#include "settings/settings.h"
#include "settings_store.h"

enum {
    START_MS = 1000,
    KEY_HOLD_MS = 30,
    HOME_MUSIC_INDEX = 0,
    HOME_SETTINGS_INDEX = 1,
    MUSIC_ALBUMS_INDEX = 3,
    SETTINGS_POWER_SAVE_INDEX = 12,
    SETTINGS_AUTO_POWER_SAVE_INDEX = 13,
    TIMEOUT_10S = 10000,
    TIMEOUT_15S = 15000,
    TIMEOUT_30S = 30000
};

static uint32_t now_ms;

static void step(void)
{
    fake_hal_set_milliseconds(now_ms);
    app_step();
}

static void advance_time(uint32_t milliseconds)
{
    uint32_t end_ms = now_ms + milliseconds;
    while (now_ms < end_ms) {
        now_ms += 10;
        step();
    }
}

static void start_app_with_library(void)
{
    uint32_t size = 0;
    uint32_t covers_size = 0;
    const uint8_t *bytes = fixture_bytes(&size);
    const uint8_t *covers = fixture_covers_bytes(&covers_size);
    fake_hal_reset();
    fake_hal_add_file("/.echo/library.db", bytes, size);
    fake_hal_add_file("/.echo/covers.bin", covers, covers_size);
    fake_hal_add_file("/.echo/build.tmp", NULL, 16 * 1024 * 1024);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
    step();
}

static void tap(Key key)
{
    fake_hal_queue_transition(key, true, now_ms);
    fake_hal_queue_transition(key, false, now_ms + KEY_HOLD_MS);
    now_ms += KEY_HOLD_MS + 10;
    step();
}

static void hold_back(void)
{
    fake_hal_queue_transition(KEY_BACK, true, now_ms);
    now_ms += 1000;
    step();
    fake_hal_queue_transition(KEY_BACK, false, now_ms);
    now_ms += 10;
    step();
}

static void move_down(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_UP);
    }
}

static void move_up(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_DOWN);
    }
}

static bool title_is(const char *expected)
{
    return strcmp(expected, app_screen_title()) == 0;
}

static void open_music_entry(int index)
{
    tap(KEY_PLAY);
    move_down(index);
    tap(KEY_PLAY);
}

static void play_first_album_track(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_PLAY);
}

static void open_setting_from_home(int index)
{
    move_down(1);
    tap(KEY_PLAY);
    move_down(index);
    tap(KEY_PLAY);
}

static bool saved_settings(Settings *settings)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(SETTINGS_STORE_PATH, &size);
    return bytes != NULL && settings_decode(bytes, size, settings);
}

static void test_enable_disable_each_mode(void)
{
    start_app_with_library();
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());

    open_setting_from_home(SETTINGS_POWER_SAVE_INDEX);
    CHECK(title_is("Power saving"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());

    tap(KEY_PLAY);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());

    tap(KEY_PLAY);
    move_up(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());
}

static void test_switch_modes_during_playback(void)
{
    start_app_with_library();
    play_first_album_track();
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK(app_player_status().has_track);

    hold_back();
    move_down(1);
    tap(KEY_PLAY);
    move_down(SETTINGS_POWER_SAVE_INDEX);
    tap(KEY_PLAY);

    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK(app_player_status().has_track);

    tap(KEY_PLAY);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK(app_player_status().has_track);

    tap(KEY_PLAY);
    move_up(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK(app_player_status().has_track);
}

static void test_settings_persistence_and_restoration(void)
{
    start_app_with_library();
    open_setting_from_home(SETTINGS_POWER_SAVE_INDEX);
    move_down(1);
    tap(KEY_PLAY);

    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Auto power save"));
    move_down(2);
    tap(KEY_PLAY);

    Settings saved;
    CHECK(saved_settings(&saved));
    CHECK_EQUAL(POWER_SAVE_BALANCED, saved.power_save_mode);
    CHECK_EQUAL(AUTO_POWER_SAVE_20_PERCENT, saved.auto_power_save);

    app_stop();
    app_start();
    step();
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());
}

static void test_display_timeout_restoration(void)
{
    start_app_with_library();
    CHECK(fake_hal_display_on());

    open_setting_from_home(SETTINGS_POWER_SAVE_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());

    advance_time(TIMEOUT_15S - 1000);
    CHECK(fake_hal_display_on());
    advance_time(2000);
    CHECK(!fake_hal_display_on());

    tap(KEY_POWER);
    CHECK(fake_hal_display_on());

    tap(KEY_PLAY);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());

    advance_time(TIMEOUT_10S - 1000);
    CHECK(fake_hal_display_on());
    advance_time(2000);
    CHECK(!fake_hal_display_on());

    tap(KEY_POWER);
    CHECK(fake_hal_display_on());

    tap(KEY_PLAY);
    move_up(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());

    advance_time(TIMEOUT_15S);
    CHECK(fake_hal_display_on());
    advance_time(TIMEOUT_15S + 1000);
    CHECK(!fake_hal_display_on());
}

static void test_screen_wake_on_key_press(void)
{
    start_app_with_library();
    open_setting_from_home(SETTINGS_POWER_SAVE_INDEX);
    move_down(2);
    tap(KEY_PLAY);

    advance_time(TIMEOUT_10S + 1000);
    CHECK(!fake_hal_display_on());

    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
}

static void test_auto_power_save_low_battery_hysteresis(void)
{
    start_app_with_library();
    open_setting_from_home(SETTINGS_AUTO_POWER_SAVE_INDEX);
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());

    HalBattery battery = {.known = true, .percent = 25, .charging = false, .usb_power = false, .millivolts = 3700};
    fake_hal_set_battery(battery);
    advance_time(500);
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());

    battery.percent = 18;
    fake_hal_set_battery(battery);
    advance_time(500);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());
    CHECK(app_toast_text() != NULL);

    battery.percent = 22;
    fake_hal_set_battery(battery);
    advance_time(500);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());

    battery.charging = true;
    fake_hal_set_battery(battery);
    advance_time(500);
    CHECK_EQUAL(POWER_SAVE_OFF, app_power_save_mode());
}

static void test_aac_and_opus_codecs_playback(void)
{
    start_app_with_library();
    play_first_album_track();
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);

    hold_back();
    move_down(1);
    tap(KEY_PLAY);
    move_down(SETTINGS_POWER_SAVE_INDEX);
    tap(KEY_PLAY);
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());

    HalAudioFormat aac_format = {.codec = HAL_AUDIO_CODEC_AAC, .sample_rate = 44100, .channels = 2, .bits_per_sample = 16};
    fake_hal_audio_set_format(aac_format);
    step();
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK_EQUAL(HAL_AUDIO_CODEC_AAC, app_player_status().format.codec);

    HalAudioFormat opus_format = {.codec = HAL_AUDIO_CODEC_OPUS, .sample_rate = 48000, .channels = 2, .bits_per_sample = 16};
    fake_hal_audio_set_format(opus_format);
    step();
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK_EQUAL(HAL_AUDIO_CODEC_OPUS, app_player_status().format.codec);
}

static void test_gapless_and_pre_decoding(void)
{
    start_app_with_library();
    play_first_album_track();
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);

    hold_back();
    move_down(1);
    tap(KEY_PLAY);
    move_down(SETTINGS_POWER_SAVE_INDEX);
    tap(KEY_PLAY);
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());

    const char *first_track = fake_hal_audio_playing_path();
    CHECK(first_track != NULL);
    const char *next_track = fake_hal_audio_queued_path();
    CHECK(next_track != NULL);
    CHECK(strcmp(first_track, next_track) != 0);

    fake_hal_audio_finish_track();
    advance_time(100);
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK(fake_hal_audio_playing_path() != NULL);
    CHECK(strcmp(next_track, fake_hal_audio_playing_path()) == 0);
}

static void test_menu_navigation_during_playback(void)
{
    start_app_with_library();
    play_first_album_track();
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);

    hold_back();
    move_down(1);
    tap(KEY_PLAY);
    move_down(SETTINGS_POWER_SAVE_INDEX);
    tap(KEY_PLAY);
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());

    hold_back();
    for (int index = 0; index < 6; index++) {
        advance_time(3000);
        CHECK(fake_hal_display_on());
        tap(KEY_VOLUME_UP);
        CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    }
    CHECK(fake_hal_display_on());
}

static void test_library_rescan_during_power_saving(void)
{
    start_app_with_library();
    open_setting_from_home(SETTINGS_POWER_SAVE_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());

    app_request_rescan();
    step();
    CHECK(fake_hal_display_on());
    CHECK(app_toast_text() != NULL);
    CHECK_EQUAL(POWER_SAVE_BALANCED, app_power_save_mode());
}

static void test_background_activity_and_no_timer_leaks(void)
{
    start_app_with_library();
    play_first_album_track();

    int writes_start = fake_hal_write_count();
    advance_time(125000);
    int writes_off = fake_hal_write_count() - writes_start;

    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    hold_back();
    move_down(1);
    tap(KEY_PLAY);
    move_down(SETTINGS_POWER_SAVE_INDEX);
    tap(KEY_PLAY);
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, app_power_save_mode());

    for (int cycle = 0; cycle < 5; cycle++) {
        tap(KEY_PLAY);
        move_up(1);
        tap(KEY_PLAY);
        tap(KEY_PLAY);
        move_down(1);
        tap(KEY_PLAY);
    }

    writes_start = fake_hal_write_count();
    advance_time(125000);
    int writes_maximum = fake_hal_write_count() - writes_start;

    CHECK(writes_maximum < writes_off);
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(test_enable_disable_each_mode),
        TEST_CASE(test_switch_modes_during_playback),
        TEST_CASE(test_settings_persistence_and_restoration),
        TEST_CASE(test_display_timeout_restoration),
        TEST_CASE(test_screen_wake_on_key_press),
        TEST_CASE(test_auto_power_save_low_battery_hysteresis),
        TEST_CASE(test_aac_and_opus_codecs_playback),
        TEST_CASE(test_gapless_and_pre_decoding),
        TEST_CASE(test_menu_navigation_during_playback),
        TEST_CASE(test_library_rescan_during_power_saving),
        TEST_CASE(test_background_activity_and_no_timer_leaks),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
