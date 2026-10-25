#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    BOOT_POWER_HOLD_MS = 1500
};

typedef struct BootPower {
    uint32_t hold_ms;
    uint32_t elapsed_ms;
    bool usb_power;
    bool confirmed;
    bool aborted;
} BootPower;

void boot_power_init(BootPower *power, uint32_t hold_ms, bool usb_power);
bool boot_power_update(BootPower *power, bool key_down, uint32_t delta_ms);
bool boot_power_confirmed(const BootPower *power);
