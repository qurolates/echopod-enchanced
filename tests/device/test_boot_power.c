#include "check.h"
#include "device/boot_power.h"

static void usb_power_confirms_immediately_without_key_press(void)
{
    BootPower power;
    boot_power_init(&power, 1500, true);
    CHECK(boot_power_confirmed(&power));
    CHECK(boot_power_update(&power, false, 100));
}

static void battery_boot_requires_key_held_for_full_threshold(void)
{
    BootPower power;
    boot_power_init(&power, 1500, false);
    CHECK(!boot_power_confirmed(&power));
    CHECK(boot_power_update(&power, true, 500));
    CHECK(!boot_power_confirmed(&power));
    CHECK(boot_power_update(&power, true, 500));
    CHECK(!boot_power_confirmed(&power));
    CHECK(boot_power_update(&power, true, 500));
    CHECK(boot_power_confirmed(&power));
}

static void releasing_key_early_aborts_boot(void)
{
    BootPower power;
    boot_power_init(&power, 1500, false);
    CHECK(boot_power_update(&power, true, 400));
    CHECK(!boot_power_confirmed(&power));
    CHECK(!boot_power_update(&power, false, 10));
    CHECK(!boot_power_confirmed(&power));
    CHECK(!boot_power_update(&power, true, 2000));
}

static void initial_key_up_aborts_immediately(void)
{
    BootPower power;
    boot_power_init(&power, 1500, false);
    CHECK(!boot_power_update(&power, false, 0));
    CHECK(!boot_power_confirmed(&power));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(usb_power_confirms_immediately_without_key_press),
        TEST_CASE(battery_boot_requires_key_held_for_full_threshold),
        TEST_CASE(releasing_key_early_aborts_boot),
        TEST_CASE(initial_key_up_aborts_immediately),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
