#include "device/boot_power.h"

void boot_power_init(BootPower *power, uint32_t hold_ms, bool usb_power)
{
    *power = (BootPower){
        .hold_ms = hold_ms,
        .elapsed_ms = 0,
        .usb_power = usb_power,
        .confirmed = usb_power,
        .aborted = false,
    };
}

bool boot_power_update(BootPower *power, bool key_down, uint32_t delta_ms)
{
    if (power->confirmed) {
        return true;
    }
    if (power->aborted || !key_down) {
        power->aborted = true;
        return false;
    }
    power->elapsed_ms += delta_ms;
    if (power->elapsed_ms >= power->hold_ms) {
        power->confirmed = true;
    }
    return true;
}

bool boot_power_confirmed(const BootPower *power)
{
    return power->confirmed;
}
