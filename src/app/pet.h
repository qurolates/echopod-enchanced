#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "player.h"

enum {
    PET_FORMAT_VERSION = 1,
    PET_STATE_SIZE = 24,
    PET_REACTION_DURATION_MS = 2000,
    PET_AFFECTION_MAX = 100,
    PET_STAGE_BABY_THRESHOLD_MS = 30 * 60 * 1000,
    PET_STAGE_CHILD_THRESHOLD_MS = 3 * 3600 * 1000,
    PET_STAGE_ADULT_THRESHOLD_MS = 12 * 3600 * 1000,
    PET_MAX_STEP_MS = 1000,
    PET_SAVE_INTERVAL_MS = 30000
};

typedef enum PetState {
    PET_STATE_IDLE,
    PET_STATE_LISTENING,
    PET_STATE_SLEEPING,
    PET_STATE_REACTION
} PetState;

typedef enum PetStage {
    PET_STAGE_EGG,
    PET_STAGE_BABY,
    PET_STAGE_CHILD,
    PET_STAGE_ADULT
} PetStage;

typedef enum PetAudioEvent {
    PET_AUDIO_NONE,
    PET_AUDIO_PLAY_STARTED,
    PET_AUDIO_PLAY_PAUSED,
    PET_AUDIO_PLAY_STOPPED,
    PET_AUDIO_TRACK_CHANGED,
    PET_AUDIO_POSITION_ADVANCED
} PetAudioEvent;

typedef struct PetAudioSnapshot {
    PlayerState state;
    bool has_track;
    uint16_t track;
    uint32_t position_ms;
} PetAudioSnapshot;

typedef struct PetAudioObserver {
    PetAudioSnapshot previous;
    bool initialized;
} PetAudioObserver;

typedef struct PetModel {
    uint16_t version;
    PetStage stage;
    uint32_t listening_time_ms;
    uint16_t affection;
    uint32_t interaction_count;
} PetModel;

typedef struct Pet {
    PetModel model;
    PetState state;
    PetAudioObserver audio_observer;
    uint32_t reaction_until_ms;
    uint32_t last_update_ms;
    uint32_t last_playing_ms;
    uint32_t last_position_ms;
    uint16_t last_track;
    bool was_playing;
    bool screen_active;
    bool dirty;
    uint32_t last_save_ms;
} Pet;

extern const char PET_STORE_PATH[];

void pet_init(Pet *pet);
bool pet_load(Pet *pet);
bool pet_save(Pet *pet);
bool pet_save_periodically(Pet *pet, uint32_t now_ms);
PetAudioEvent pet_handle_playback(Pet *pet, PlayerStatus status);
void pet_update(Pet *pet, PlayerStatus status, uint32_t now_ms);
void pet_interact(Pet *pet, uint32_t now_ms);
void pet_set_screen_active(Pet *pet, bool active, PlayerStatus status, uint32_t now_ms);
PetState pet_state(const Pet *pet);
const char *pet_state_name(PetState state);
const char *pet_state_description(PetState state);
PetStage pet_stage_for_time(uint32_t listening_time_ms);
const char *pet_stage_name(PetStage stage);
uint32_t pet_next_stage_threshold(PetStage stage);
int pet_stage_progress_percent(const PetModel *model);
void pet_format_duration(uint32_t milliseconds, char *buffer, size_t capacity);
void pet_model_init(PetModel *model);
void pet_model_encode(const PetModel *model, uint8_t *bytes);
bool pet_model_decode(const uint8_t *bytes, uint32_t length, PetModel *model);
bool pet_model_save(const PetModel *model);
bool pet_model_load(PetModel *model);
