#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum PowerSaveMode {
    POWER_SAVE_OFF,
    POWER_SAVE_BALANCED,
    POWER_SAVE_MAXIMUM
} PowerSaveMode;

typedef enum AutoPowerSaveMode {
    AUTO_POWER_SAVE_OFF,
    AUTO_POWER_SAVE_10_PERCENT,
    AUTO_POWER_SAVE_20_PERCENT
} AutoPowerSaveMode;

enum {
    POWER_SAVE_MODE_COUNT = POWER_SAVE_MAXIMUM + 1,
    AUTO_POWER_SAVE_MODE_COUNT = AUTO_POWER_SAVE_20_PERCENT + 1,
    POWER_SAVE_BALANCED_TIMEOUT_MS = 15000,
    POWER_SAVE_MAXIMUM_TIMEOUT_MS = 10000,
    POWER_SAVE_HYSTERESIS_PERCENT = 5,
    POWER_SAVE_PLAYER_SAVE_OFF_MS = 15000,
    POWER_SAVE_PLAYER_SAVE_BALANCED_MS = 60000,
    POWER_SAVE_PLAYER_SAVE_MAXIMUM_MS = 120000,
    POWER_SAVE_PET_SAVE_OFF_MS = 30000,
    POWER_SAVE_PET_SAVE_BALANCED_MS = 60000,
    POWER_SAVE_PET_SAVE_MAXIMUM_MS = 120000,
    POWER_SAVE_FRAME_INTERVAL_OFF_MS = 0,
    POWER_SAVE_FRAME_INTERVAL_BALANCED_MS = 33,
    POWER_SAVE_FRAME_INTERVAL_MAXIMUM_MS = 66,
    POWER_SAVE_IDLE_INTERVAL_BALANCED_MS = 100,
    POWER_SAVE_IDLE_INTERVAL_MAXIMUM_MS = 250
};

typedef struct PowerSaveManager {
    PowerSaveMode configured_mode;
    AutoPowerSaveMode auto_mode;
    PowerSaveMode active_mode;
    bool auto_active;
    uint32_t user_timeout_ms;
} PowerSaveManager;

void power_save_init(PowerSaveManager *mgr, PowerSaveMode mode, AutoPowerSaveMode auto_mode,
                     uint32_t user_timeout_ms);
void power_save_set_mode(PowerSaveManager *mgr, PowerSaveMode mode);
void power_save_set_auto_mode(PowerSaveManager *mgr, AutoPowerSaveMode auto_mode);
void power_save_set_user_timeout(PowerSaveManager *mgr, uint32_t user_timeout_ms);
PowerSaveMode power_save_active_mode(const PowerSaveManager *mgr);
bool power_save_is_auto_active(const PowerSaveManager *mgr);
uint32_t power_save_effective_timeout(const PowerSaveManager *mgr);
bool power_save_update_battery(PowerSaveManager *mgr, bool known, uint8_t percent, bool charging,
                               bool usb_power);
bool power_save_animations_enabled(const PowerSaveManager *mgr);
bool power_save_marquee_enabled(const PowerSaveManager *mgr);
uint32_t power_save_frame_interval_ms(const PowerSaveManager *mgr, bool animating);
uint32_t power_save_player_save_interval_ms(const PowerSaveManager *mgr);
uint32_t power_save_pet_save_interval_ms(const PowerSaveManager *mgr);
const char *power_save_mode_name(PowerSaveMode mode);
const char *power_save_auto_mode_name(AutoPowerSaveMode mode);
