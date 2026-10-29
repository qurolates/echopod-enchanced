#include "pet.h"

#include "core/hash.h"
#include "hal/storage.h"

const char PET_STORE_PATH[] = "/.echo/echopet.bin";

enum {
    MAGIC_FIELD = 0,
    VERSION_FIELD = 4,
    STAGE_FIELD = 6,
    LISTENING_TIME_FIELD = 8,
    AFFECTION_FIELD = 12,
    INTERACTION_COUNT_FIELD = 14,
    RESERVED_FIELD = 18,
    CHECKSUM_FIELD = 20,
    MAGIC_SIZE = 4,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'P', 'E', 'T'};

static void put_u16(uint8_t *bytes, int offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> BITS_PER_BYTE);
}

static void put_u32(uint8_t *bytes, int offset, uint32_t value)
{
    put_u16(bytes, offset, (uint16_t)(value & 0xFFFFu));
    put_u16(bytes, offset + 2, (uint16_t)(value >> 16));
}

static uint16_t get_u16(const uint8_t *bytes, int offset)
{
    return (uint16_t)(bytes[offset] | (bytes[offset + 1] << BITS_PER_BYTE));
}

static uint32_t get_u32(const uint8_t *bytes, int offset)
{
    return (uint32_t)get_u16(bytes, offset) | ((uint32_t)get_u16(bytes, offset + 2) << 16);
}

void pet_model_init(PetModel *model)
{
    *model = (PetModel){
        .version = PET_FORMAT_VERSION,
        .stage = PET_STAGE_BABY,
        .listening_time_ms = 0,
        .affection = 0,
        .interaction_count = 0,
    };
}

void pet_model_encode(const PetModel *model, uint8_t *bytes)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        bytes[MAGIC_FIELD + index] = MAGIC[index];
    }
    put_u16(bytes, VERSION_FIELD, model->version);
    put_u16(bytes, STAGE_FIELD, (uint16_t)model->stage);
    put_u32(bytes, LISTENING_TIME_FIELD, model->listening_time_ms);
    put_u16(bytes, AFFECTION_FIELD, model->affection);
    put_u32(bytes, INTERACTION_COUNT_FIELD, model->interaction_count);
    put_u16(bytes, RESERVED_FIELD, 0);
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    put_u32(bytes, CHECKSUM_FIELD, checksum);
}

bool pet_model_decode(const uint8_t *bytes, uint32_t length, PetModel *model)
{
    if (length < PET_STATE_SIZE) {
        return false;
    }
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (bytes[MAGIC_FIELD + index] != MAGIC[index]) {
            return false;
        }
    }
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    if (get_u32(bytes, CHECKSUM_FIELD) != checksum) {
        return false;
    }
    uint16_t version = get_u16(bytes, VERSION_FIELD);
    if (version == 0 || version > PET_FORMAT_VERSION) {
        return false;
    }
    model->version = version;
    model->stage = (PetStage)get_u16(bytes, STAGE_FIELD);
    model->listening_time_ms = get_u32(bytes, LISTENING_TIME_FIELD);
    model->affection = get_u16(bytes, AFFECTION_FIELD);
    model->interaction_count = get_u32(bytes, INTERACTION_COUNT_FIELD);
    return true;
}

bool pet_model_save(const PetModel *model)
{
    uint8_t bytes[PET_STATE_SIZE];
    pet_model_encode(model, bytes);
    return hal_file_write(PET_STORE_PATH, bytes, sizeof bytes);
}

bool pet_model_load(PetModel *model)
{
    HalFile file;
    if (!hal_file_open(&file, PET_STORE_PATH)) {
        return false;
    }
    uint8_t bytes[PET_STATE_SIZE];
    bool read = file.size >= PET_STATE_SIZE && hal_file_read(file, 0, bytes, sizeof bytes);
    hal_file_close(&file);
    if (!read) {
        return false;
    }
    return pet_model_decode(bytes, sizeof bytes, model);
}

PetAudioEvent pet_handle_playback(Pet *pet, PlayerStatus status)
{
    PetAudioObserver *observer = &pet->audio_observer;
    if (!observer->initialized) {
        observer->previous = (PetAudioSnapshot){
            .state = status.state,
            .has_track = status.has_track,
            .track = status.track,
            .position_ms = status.position_ms,
        };
        observer->initialized = true;
        return PET_AUDIO_NONE;
    }
    PetAudioEvent event = PET_AUDIO_NONE;
    PetAudioSnapshot prev = observer->previous;
    if (status.has_track != prev.has_track || (status.has_track && status.track != prev.track)) {
        event = PET_AUDIO_TRACK_CHANGED;
    } else if (status.state == PLAYER_PLAYING && prev.state != PLAYER_PLAYING) {
        event = PET_AUDIO_PLAY_STARTED;
    } else if (status.state == PLAYER_PAUSED && prev.state == PLAYER_PLAYING) {
        event = PET_AUDIO_PLAY_PAUSED;
    } else if (status.state == PLAYER_IDLE && prev.state != PLAYER_IDLE) {
        event = PET_AUDIO_PLAY_STOPPED;
    } else if (status.state == PLAYER_PLAYING && status.position_ms > prev.position_ms) {
        event = PET_AUDIO_POSITION_ADVANCED;
    }
    observer->previous = (PetAudioSnapshot){
        .state = status.state,
        .has_track = status.has_track,
        .track = status.track,
        .position_ms = status.position_ms,
    };
    return event;
}

void pet_init(Pet *pet)
{
    pet_model_init(&pet->model);
    pet->state = PET_STATE_IDLE;
    pet->audio_observer = (PetAudioObserver){.initialized = false};
    pet->reaction_until_ms = 0;
    pet->last_update_ms = 0;
    pet->screen_active = false;
}

void pet_update(Pet *pet, PlayerStatus status, uint32_t now_ms)
{
    pet_handle_playback(pet, status);
    pet->last_update_ms = now_ms;
    if (!pet->screen_active) {
        pet->state = PET_STATE_SLEEPING;
        pet->reaction_until_ms = 0;
        return;
    }
    if (pet->reaction_until_ms > 0) {
        if (now_ms < pet->reaction_until_ms) {
            pet->state = PET_STATE_REACTION;
            return;
        }
        pet->reaction_until_ms = 0;
    }
    if (status.state == PLAYER_PLAYING) {
        pet->state = PET_STATE_LISTENING;
    } else {
        pet->state = PET_STATE_IDLE;
    }
}

void pet_interact(Pet *pet, uint32_t now_ms)
{
    if (!pet->screen_active) {
        return;
    }
    pet->reaction_until_ms = now_ms + PET_REACTION_DURATION_MS;
    pet->state = PET_STATE_REACTION;
    pet->model.interaction_count++;
    if (pet->model.affection < PET_AFFECTION_MAX) {
        pet->model.affection++;
    }
}

void pet_set_screen_active(Pet *pet, bool active, PlayerStatus status, uint32_t now_ms)
{
    pet->screen_active = active;
    pet->reaction_until_ms = 0;
    pet_update(pet, status, now_ms);
}

PetState pet_state(const Pet *pet)
{
    return pet->state;
}

const char *pet_state_name(PetState state)
{
    switch (state) {
    case PET_STATE_IDLE:
        return "IDLE";
    case PET_STATE_LISTENING:
        return "LISTENING";
    case PET_STATE_SLEEPING:
        return "SLEEPING";
    case PET_STATE_REACTION:
        return "HAPPY!";
    }
    return "UNKNOWN";
}

const char *pet_state_description(PetState state)
{
    switch (state) {
    case PET_STATE_IDLE:
        return "Resting peacefully";
    case PET_STATE_LISTENING:
        return "Jamming to music!";
    case PET_STATE_SLEEPING:
        return "Fast asleep... Zzz";
    case PET_STATE_REACTION:
        return "Purring happily! <3";
    }
    return "";
}
