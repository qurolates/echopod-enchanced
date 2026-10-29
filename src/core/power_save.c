#include "core/power_save.h"

static const char *const MODE_NAMES[POWER_SAVE_MODE_COUNT] = {"Off", "Balanced", "Maximum"};

static const char *const AUTO_MODE_NAMES[AUTO_POWER_SAVE_MODE_COUNT] = {"Off", "10%", "20%"};

static uint8_t auto_threshold_percent(AutoPowerSaveMode mode)
{
    switch (mode) {
    case AUTO_POWER_SAVE_10_PERCENT:
        return 10;
    case AUTO_POWER_SAVE_20_PERCENT:
        return 20;
    case AUTO_POWER_SAVE_OFF:
    default:
        return 0;
    }
}

void power_save_init(PowerSaveManager *mgr, PowerSaveMode mode, AutoPowerSaveMode auto_mode,
                     uint32_t user_timeout_ms)
{
    mgr->configured_mode = mode;
    mgr->auto_mode = auto_mode;
    mgr->active_mode = mode;
    mgr->auto_active = false;
    mgr->user_timeout_ms = user_timeout_ms;
}

void power_save_set_mode(PowerSaveManager *mgr, PowerSaveMode mode)
{
    mgr->configured_mode = mode;
    if (!mgr->auto_active) {
        mgr->active_mode = mode;
    } else if (mode > mgr->active_mode) {
        mgr->active_mode = mode;
    }
}

void power_save_set_auto_mode(PowerSaveManager *mgr, AutoPowerSaveMode auto_mode)
{
    mgr->auto_mode = auto_mode;
    if (auto_mode == AUTO_POWER_SAVE_OFF && mgr->auto_active) {
        mgr->auto_active = false;
        mgr->active_mode = mgr->configured_mode;
    }
}

void power_save_set_user_timeout(PowerSaveManager *mgr, uint32_t user_timeout_ms)
{
    mgr->user_timeout_ms = user_timeout_ms;
}

PowerSaveMode power_save_active_mode(const PowerSaveManager *mgr)
{
    return mgr->active_mode;
}

bool power_save_is_auto_active(const PowerSaveManager *mgr)
{
    return mgr->auto_active;
}

uint32_t power_save_effective_timeout(const PowerSaveManager *mgr)
{
    switch (mgr->active_mode) {
    case POWER_SAVE_BALANCED:
        return (mgr->user_timeout_ms > 0 &&
                mgr->user_timeout_ms < POWER_SAVE_BALANCED_TIMEOUT_MS)
                   ? mgr->user_timeout_ms
                   : POWER_SAVE_BALANCED_TIMEOUT_MS;
    case POWER_SAVE_MAXIMUM:
        return (mgr->user_timeout_ms > 0 &&
                mgr->user_timeout_ms < POWER_SAVE_MAXIMUM_TIMEOUT_MS)
                   ? mgr->user_timeout_ms
                   : POWER_SAVE_MAXIMUM_TIMEOUT_MS;
    case POWER_SAVE_OFF:
    default:
        return mgr->user_timeout_ms;
    }
}

bool power_save_update_battery(PowerSaveManager *mgr, bool known, uint8_t percent, bool charging,
                               bool usb_power)
{
    if (mgr->auto_mode == AUTO_POWER_SAVE_OFF) {
        if (mgr->auto_active) {
            mgr->auto_active = false;
            mgr->active_mode = mgr->configured_mode;
            return true;
        }
        return false;
    }
    if (!known) {
        return false;
    }
    if (charging || usb_power) {
        if (mgr->auto_active) {
            mgr->auto_active = false;
            mgr->active_mode = mgr->configured_mode;
            return true;
        }
        return false;
    }
    uint8_t threshold = auto_threshold_percent(mgr->auto_mode);
    if (!mgr->auto_active) {
        if (percent <= threshold) {
            mgr->auto_active = true;
            PowerSaveMode target = (threshold <= 10 || percent <= 10)
                                       ? POWER_SAVE_MAXIMUM
                                       : POWER_SAVE_BALANCED;
            if (mgr->configured_mode > target) {
                target = mgr->configured_mode;
            }
            mgr->active_mode = target;
            return true;
        }
    } else {
        if (percent >= (uint8_t)(threshold + POWER_SAVE_HYSTERESIS_PERCENT)) {
            mgr->auto_active = false;
            mgr->active_mode = mgr->configured_mode;
            return true;
        }
    }
    return false;
}

bool power_save_animations_enabled(const PowerSaveManager *mgr)
{
    return mgr->active_mode != POWER_SAVE_MAXIMUM;
}

bool power_save_marquee_enabled(const PowerSaveManager *mgr)
{
    return mgr->active_mode != POWER_SAVE_MAXIMUM;
}

uint32_t power_save_frame_interval_ms(const PowerSaveManager *mgr, bool animating)
{
    switch (mgr->active_mode) {
    case POWER_SAVE_BALANCED:
        return animating ? POWER_SAVE_FRAME_INTERVAL_BALANCED_MS
                         : POWER_SAVE_IDLE_INTERVAL_BALANCED_MS;
    case POWER_SAVE_MAXIMUM:
        return animating ? POWER_SAVE_FRAME_INTERVAL_MAXIMUM_MS
                         : POWER_SAVE_IDLE_INTERVAL_MAXIMUM_MS;
    case POWER_SAVE_OFF:
    default:
        return POWER_SAVE_FRAME_INTERVAL_OFF_MS;
    }
}

uint32_t power_save_player_save_interval_ms(const PowerSaveManager *mgr)
{
    switch (mgr->active_mode) {
    case POWER_SAVE_BALANCED:
        return POWER_SAVE_PLAYER_SAVE_BALANCED_MS;
    case POWER_SAVE_MAXIMUM:
        return POWER_SAVE_PLAYER_SAVE_MAXIMUM_MS;
    case POWER_SAVE_OFF:
    default:
        return POWER_SAVE_PLAYER_SAVE_OFF_MS;
    }
}

uint32_t power_save_pet_save_interval_ms(const PowerSaveManager *mgr)
{
    switch (mgr->active_mode) {
    case POWER_SAVE_BALANCED:
        return POWER_SAVE_PET_SAVE_BALANCED_MS;
    case POWER_SAVE_MAXIMUM:
        return POWER_SAVE_PET_SAVE_MAXIMUM_MS;
    case POWER_SAVE_OFF:
    default:
        return POWER_SAVE_PET_SAVE_OFF_MS;
    }
}

const char *power_save_mode_name(PowerSaveMode mode)
{
    return (int)mode < (int)POWER_SAVE_MODE_COUNT ? MODE_NAMES[mode] : "";
}

const char *power_save_auto_mode_name(AutoPowerSaveMode mode)
{
    return (int)mode < (int)AUTO_POWER_SAVE_MODE_COUNT ? AUTO_MODE_NAMES[mode] : "";
}
