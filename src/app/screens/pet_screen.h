#pragma once

#include <stdint.h>

#include "pet.h"
#include "player.h"
#include "ui/screen.h"
#include "ui/theme.h"

typedef struct PetScreen {
    Screen screen;
    const Theme *theme;
    const Player *player;
    Pet pet;
    uint32_t now_ms;
} PetScreen;

Screen *pet_screen_init(PetScreen *screen, const Theme *theme, const Player *player);
Pet *pet_screen_pet(PetScreen *screen);
void pet_screen_interact(PetScreen *screen, uint32_t now_ms);
