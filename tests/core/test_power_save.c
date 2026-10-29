#include "check.h"
#include "core/power_save.h"

enum {
    USER_TIMEOUT_MS = 30000,
    SHORT_USER_TIMEOUT_MS = 5000,
    NEVER_TIMEOUT_MS = 0
};

static void manager_initializes_with_configured_values(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_OFF, USER_TIMEOUT_MS);
    CHECK_EQUAL(POWER_SAVE_OFF, power_save_active_mode(&mgr));
    CHECK(!power_save_is_auto_active(&mgr));
    CHECK_EQUAL(USER_TIMEOUT_MS, power_save_effective_timeout(&mgr));
    CHECK(power_save_animations_enabled(&mgr));
    CHECK(power_save_marquee_enabled(&mgr));
    CHECK_EQUAL(POWER_SAVE_PLAYER_SAVE_OFF_MS, power_save_player_save_interval_ms(&mgr));
    CHECK_EQUAL(POWER_SAVE_PET_SAVE_OFF_MS, power_save_pet_save_interval_ms(&mgr));
    CHECK_EQUAL(POWER_SAVE_FRAME_INTERVAL_OFF_MS, power_save_frame_interval_ms(&mgr, false));
    CHECK_EQUAL(POWER_SAVE_FRAME_INTERVAL_OFF_MS, power_save_frame_interval_ms(&mgr, true));
}

static void balanced_mode_applies_shorter_timeout_and_intervals(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_BALANCED, AUTO_POWER_SAVE_OFF, USER_TIMEOUT_MS);
    CHECK_EQUAL(POWER_SAVE_BALANCED, power_save_active_mode(&mgr));
    CHECK_EQUAL(POWER_SAVE_BALANCED_TIMEOUT_MS, power_save_effective_timeout(&mgr));
    CHECK(power_save_animations_enabled(&mgr));
    CHECK(power_save_marquee_enabled(&mgr));
    CHECK_EQUAL(POWER_SAVE_PLAYER_SAVE_BALANCED_MS, power_save_player_save_interval_ms(&mgr));
    CHECK_EQUAL(POWER_SAVE_PET_SAVE_BALANCED_MS, power_save_pet_save_interval_ms(&mgr));
    CHECK_EQUAL(POWER_SAVE_FRAME_INTERVAL_BALANCED_MS, power_save_frame_interval_ms(&mgr, true));
    CHECK_EQUAL(POWER_SAVE_IDLE_INTERVAL_BALANCED_MS, power_save_frame_interval_ms(&mgr, false));
}

static void maximum_mode_disables_animations_and_minimizes_activity(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_MAXIMUM, AUTO_POWER_SAVE_OFF, USER_TIMEOUT_MS);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, power_save_active_mode(&mgr));
    CHECK_EQUAL(POWER_SAVE_MAXIMUM_TIMEOUT_MS, power_save_effective_timeout(&mgr));
    CHECK(!power_save_animations_enabled(&mgr));
    CHECK(!power_save_marquee_enabled(&mgr));
    CHECK_EQUAL(POWER_SAVE_PLAYER_SAVE_MAXIMUM_MS, power_save_player_save_interval_ms(&mgr));
    CHECK_EQUAL(POWER_SAVE_PET_SAVE_MAXIMUM_MS, power_save_pet_save_interval_ms(&mgr));
    CHECK_EQUAL(POWER_SAVE_FRAME_INTERVAL_MAXIMUM_MS, power_save_frame_interval_ms(&mgr, true));
    CHECK_EQUAL(POWER_SAVE_IDLE_INTERVAL_MAXIMUM_MS, power_save_frame_interval_ms(&mgr, false));
}

static void short_user_timeout_is_respected_in_power_saving(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_BALANCED, AUTO_POWER_SAVE_OFF, SHORT_USER_TIMEOUT_MS);
    CHECK_EQUAL(SHORT_USER_TIMEOUT_MS, power_save_effective_timeout(&mgr));

    power_save_set_mode(&mgr, POWER_SAVE_MAXIMUM);
    CHECK_EQUAL(SHORT_USER_TIMEOUT_MS, power_save_effective_timeout(&mgr));
}

static void never_timeout_gets_bounded_in_power_saving(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_OFF, NEVER_TIMEOUT_MS);
    CHECK_EQUAL(NEVER_TIMEOUT_MS, power_save_effective_timeout(&mgr));

    power_save_set_mode(&mgr, POWER_SAVE_BALANCED);
    CHECK_EQUAL(POWER_SAVE_BALANCED_TIMEOUT_MS, power_save_effective_timeout(&mgr));

    power_save_set_mode(&mgr, POWER_SAVE_MAXIMUM);
    CHECK_EQUAL(POWER_SAVE_MAXIMUM_TIMEOUT_MS, power_save_effective_timeout(&mgr));

    power_save_set_mode(&mgr, POWER_SAVE_OFF);
    CHECK_EQUAL(NEVER_TIMEOUT_MS, power_save_effective_timeout(&mgr));
}

static void switching_mode_restores_user_timeout_cleanly(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_OFF, USER_TIMEOUT_MS);
    CHECK_EQUAL(USER_TIMEOUT_MS, power_save_effective_timeout(&mgr));

    power_save_set_mode(&mgr, POWER_SAVE_BALANCED);
    CHECK_EQUAL(POWER_SAVE_BALANCED_TIMEOUT_MS, power_save_effective_timeout(&mgr));

    power_save_set_mode(&mgr, POWER_SAVE_OFF);
    CHECK_EQUAL(USER_TIMEOUT_MS, power_save_effective_timeout(&mgr));
}

static void auto_mode_triggers_at_threshold_with_hysteresis(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_20_PERCENT, USER_TIMEOUT_MS);

    CHECK(!power_save_update_battery(&mgr, false, 15, false, false));
    CHECK(!power_save_is_auto_active(&mgr));

    CHECK(!power_save_update_battery(&mgr, true, 21, false, false));
    CHECK(!power_save_is_auto_active(&mgr));

    CHECK(power_save_update_battery(&mgr, true, 20, false, false));
    CHECK(power_save_is_auto_active(&mgr));
    CHECK_EQUAL(POWER_SAVE_BALANCED, power_save_active_mode(&mgr));

    CHECK(!power_save_update_battery(&mgr, true, 22, false, false));
    CHECK(power_save_is_auto_active(&mgr));

    CHECK(!power_save_update_battery(&mgr, true, 24, false, false));
    CHECK(power_save_is_auto_active(&mgr));

    CHECK(power_save_update_battery(&mgr, true, 25, false, false));
    CHECK(!power_save_is_auto_active(&mgr));
    CHECK_EQUAL(POWER_SAVE_OFF, power_save_active_mode(&mgr));
}

static void auto_mode_cancels_on_usb_power_or_charging(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_20_PERCENT, USER_TIMEOUT_MS);

    CHECK(power_save_update_battery(&mgr, true, 18, false, false));
    CHECK(power_save_is_auto_active(&mgr));

    CHECK(power_save_update_battery(&mgr, true, 18, true, false));
    CHECK(!power_save_is_auto_active(&mgr));
    CHECK_EQUAL(POWER_SAVE_OFF, power_save_active_mode(&mgr));

    CHECK(!power_save_update_battery(&mgr, true, 18, false, true));
    CHECK(!power_save_is_auto_active(&mgr));
}

static void auto_mode_10_percent_activates_maximum(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_10_PERCENT, USER_TIMEOUT_MS);

    CHECK(power_save_update_battery(&mgr, true, 10, false, false));
    CHECK(power_save_is_auto_active(&mgr));
    CHECK_EQUAL(POWER_SAVE_MAXIMUM, power_save_active_mode(&mgr));
}

static void disabling_auto_mode_reverts_active_state(void)
{
    PowerSaveManager mgr;
    power_save_init(&mgr, POWER_SAVE_OFF, AUTO_POWER_SAVE_20_PERCENT, USER_TIMEOUT_MS);
    power_save_update_battery(&mgr, true, 15, false, false);
    CHECK(power_save_is_auto_active(&mgr));

    power_save_set_auto_mode(&mgr, AUTO_POWER_SAVE_OFF);
    CHECK(!power_save_is_auto_active(&mgr));
    CHECK_EQUAL(POWER_SAVE_OFF, power_save_active_mode(&mgr));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(manager_initializes_with_configured_values),
        TEST_CASE(balanced_mode_applies_shorter_timeout_and_intervals),
        TEST_CASE(maximum_mode_disables_animations_and_minimizes_activity),
        TEST_CASE(short_user_timeout_is_respected_in_power_saving),
        TEST_CASE(never_timeout_gets_bounded_in_power_saving),
        TEST_CASE(switching_mode_restores_user_timeout_cleanly),
        TEST_CASE(auto_mode_triggers_at_threshold_with_hysteresis),
        TEST_CASE(auto_mode_cancels_on_usb_power_or_charging),
        TEST_CASE(auto_mode_10_percent_activates_maximum),
        TEST_CASE(disabling_auto_mode_reverts_active_state),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
