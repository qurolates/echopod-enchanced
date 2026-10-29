#pragma once

#include "pet.h"
#include "player.h"

PlayerStatus app_player_status(void);
const char *app_toast_text(void);
const char *app_diagnostics_value(int index);
const Pet *app_pet(void);
