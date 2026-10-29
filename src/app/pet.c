#include "pet.h"

#include "core/hash.h"
#include "core/string_builder.h"
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
    BITS_PER_BYTE = 8,
    MILLISECONDS_PER_SECOND = 1000,
    SECONDS_PER_MINUTE = 60,
    MINUTES_PER_HOUR = 60
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
        .stage = PET_STAGE_EGG,
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
    uint16_t stage_val = get_u16(bytes, STAGE_FIELD);
    if (stage_val > PET_STAGE_ADULT) {
        return false;
    }
    model->version = version;
    model->stage = (PetStage)stage_val;
    model->listening_time_ms = get_u32(bytes, LISTENING_TIME_FIELD);
    model->affection = get_u16(bytes, AFFECTION_FIELD);
    model->interaction_count = get_u32(bytes, INTERACTION_COUNT_FIELD);
    return true;
}

bool pet_model_save(const PetModel *model)
{
    uint8_t bytes[PET_STATE_SIZE];
    pet_model_encode(model, bytes);
    if (!hal_file_write(PET_STORE_PATH, bytes, sizeof bytes)) {
        if (hal_file_reserve(PET_STORE_PATH, 4096) != HAL_RESERVE_FAILED) {
            return hal_file_write(PET_STORE_PATH, bytes, sizeof bytes);
        }
        return false;
    }
    return true;
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

PetStage pet_stage_for_time(uint32_t listening_time_ms)
{
    if (listening_time_ms >= PET_STAGE_ADULT_THRESHOLD_MS) {
        return PET_STAGE_ADULT;
    }
    if (listening_time_ms >= PET_STAGE_CHILD_THRESHOLD_MS) {
        return PET_STAGE_CHILD;
    }
    if (listening_time_ms >= PET_STAGE_BABY_THRESHOLD_MS) {
        return PET_STAGE_BABY;
    }
    return PET_STAGE_EGG;
}

const char *pet_stage_name(PetStage stage)
{
    switch (stage) {
    case PET_STAGE_EGG:
        return "EGG";
    case PET_STAGE_BABY:
        return "BABY";
    case PET_STAGE_CHILD:
        return "CHILD";
    case PET_STAGE_ADULT:
        return "ADULT";
    }
    return "UNKNOWN";
}

uint32_t pet_next_stage_threshold(PetStage stage)
{
    switch (stage) {
    case PET_STAGE_EGG:
        return PET_STAGE_BABY_THRESHOLD_MS;
    case PET_STAGE_BABY:
        return PET_STAGE_CHILD_THRESHOLD_MS;
    case PET_STAGE_CHILD:
        return PET_STAGE_ADULT_THRESHOLD_MS;
    case PET_STAGE_ADULT:
        return PET_STAGE_ADULT_THRESHOLD_MS;
    }
    return 0;
}

int pet_stage_progress_percent(const PetModel *model)
{
    switch (model->stage) {
    case PET_STAGE_EGG: {
        uint64_t current = model->listening_time_ms;
        uint64_t target = PET_STAGE_BABY_THRESHOLD_MS;
        return (int)((current >= target) ? 100 : (current * 100 / target));
    }
    case PET_STAGE_BABY: {
        uint64_t start = PET_STAGE_BABY_THRESHOLD_MS;
        uint64_t target = PET_STAGE_CHILD_THRESHOLD_MS;
        if (model->listening_time_ms <= start) {
            return 0;
        }
        if (model->listening_time_ms >= target) {
            return 100;
        }
        return (int)((model->listening_time_ms - start) * 100 / (target - start));
    }
    case PET_STAGE_CHILD: {
        uint64_t start = PET_STAGE_CHILD_THRESHOLD_MS;
        uint64_t target = PET_STAGE_ADULT_THRESHOLD_MS;
        if (model->listening_time_ms <= start) {
            return 0;
        }
        if (model->listening_time_ms >= target) {
            return 100;
        }
        return (int)((model->listening_time_ms - start) * 100 / (target - start));
    }
    case PET_STAGE_ADULT:
        return 100;
    }
    return 0;
}

void pet_format_duration(uint32_t milliseconds, char *buffer, size_t capacity)
{
    uint32_t total_seconds = milliseconds / MILLISECONDS_PER_SECOND;
    uint32_t hours = total_seconds / (SECONDS_PER_MINUTE * MINUTES_PER_HOUR);
    uint32_t minutes = (total_seconds / SECONDS_PER_MINUTE) % MINUTES_PER_HOUR;
    uint32_t seconds = total_seconds % SECONDS_PER_MINUTE;
    StringBuilder builder = string_builder_make(buffer, capacity);
    if (hours > 0) {
        string_builder_append_unsigned(&builder, hours);
        string_builder_append(&builder, "h ");
        string_builder_append_unsigned(&builder, minutes);
        string_builder_append(&builder, "m");
    } else if (minutes > 0) {
        string_builder_append_unsigned(&builder, minutes);
        string_builder_append(&builder, "m ");
        string_builder_append_unsigned(&builder, seconds);
        string_builder_append(&builder, "s");
    } else {
        string_builder_append_unsigned(&builder, seconds);
        string_builder_append(&builder, "s");
    }
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
    pet->last_playing_ms = 0;
    pet->last_position_ms = 0;
    pet->last_track = 0;
    pet->was_playing = false;
    pet->screen_active = false;
    pet->dirty = false;
    pet->last_save_ms = 0;
    pet->save_interval_ms = PET_SAVE_INTERVAL_MS;
}

bool pet_load(Pet *pet)
{
    PetModel loaded;
    if (!pet_model_load(&loaded)) {
        pet_model_init(&pet->model);
        pet->dirty = false;
        return false;
    }
    PetStage stage = pet_stage_for_time(loaded.listening_time_ms);
    if (stage > loaded.stage) {
        loaded.stage = stage;
    }
    pet->model = loaded;
    pet->dirty = false;
    pet->was_playing = false;
    pet->last_playing_ms = 0;
    pet->last_position_ms = 0;
    pet->last_track = 0;
    pet->reaction_until_ms = 0;
    return true;
}

bool pet_save(Pet *pet)
{
    bool saved = pet_model_save(&pet->model);
    if (saved) {
        pet->dirty = false;
    }
    return saved;
}

bool pet_save_periodically(Pet *pet, uint32_t now_ms)
{
    if (!pet->dirty) {
        return false;
    }
    uint32_t interval = pet->save_interval_ms > 0 ? pet->save_interval_ms : PET_SAVE_INTERVAL_MS;
    if (now_ms - pet->last_save_ms < interval) {
        return false;
    }
    pet->last_save_ms = now_ms;
    return pet_save(pet);
}

void pet_set_save_interval(Pet *pet, uint32_t interval_ms)
{
    pet->save_interval_ms = interval_ms;
}

void pet_update(Pet *pet, PlayerStatus status, uint32_t now_ms)
{
    pet_handle_playback(pet, status);

    if (status.state == PLAYER_PLAYING) {
        if (!pet->was_playing) {
            pet->was_playing = true;
            pet->last_playing_ms = now_ms;
            pet->last_track = status.track;
            pet->last_position_ms = status.position_ms;
        } else if (now_ms > pet->last_playing_ms) {
            uint32_t delta_time = now_ms - pet->last_playing_ms;
            pet->last_playing_ms = now_ms;

            uint32_t delta_pos = 0;
            if (status.track == pet->last_track) {
                if (status.position_ms >= pet->last_position_ms) {
                    delta_pos = status.position_ms - pet->last_position_ms;
                } else {
                    delta_pos = 0;
                }
            } else {
                delta_pos = delta_time;
                pet->last_track = status.track;
            }
            pet->last_position_ms = status.position_ms;

            uint32_t gained = delta_time;
            if (status.track == pet->last_track && delta_pos < delta_time) {
                gained = delta_pos;
            }
            if (gained > PET_MAX_STEP_MS) {
                gained = PET_MAX_STEP_MS;
            }

            if (gained > 0) {
                pet->model.listening_time_ms += gained;
                pet->dirty = true;
                PetStage next_stage = pet_stage_for_time(pet->model.listening_time_ms);
                if (next_stage > pet->model.stage) {
                    pet->model.stage = next_stage;
                    pet_save(pet);
                }
            }
        }
    } else {
        pet->was_playing = false;
    }

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
    pet->dirty = true;
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
