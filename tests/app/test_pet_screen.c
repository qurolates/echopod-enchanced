#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "gfx/canvas.h"
#include "pet.h"
#include "screens/pet_screen.h"

enum {
    START_MS = 1000,
    BUFFER_WIDTH = 320,
    BUFFER_HEIGHT = 170
};

static PetScreen pet_screen;
static Color screen_pixels[BUFFER_WIDTH * BUFFER_HEIGHT];

static Screen *open_pet(const Player *player)
{
    fake_hal_reset();
    Screen *screen = pet_screen_init(&pet_screen, &DEFAULT_THEME, player);
    screen->enter(screen->context, START_MS);
    return screen;
}

static KeyEvent tap(Key key)
{
    return (KeyEvent){.key = key, .gesture = KEY_GESTURE_PRESS};
}

static KeyEvent hold(Key key)
{
    return (KeyEvent){.key = key, .gesture = KEY_GESTURE_LONG_PRESS};
}

static void screen_opens_and_exits_cleanly(void)
{
    Screen *screen = open_pet(NULL);
    CHECK(screen != NULL);
    CHECK(strcmp("EchoPet", screen->title) == 0);
    CHECK(screen->stays_on_wake);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(pet_screen_pet(&pet_screen)));

    ScreenAction pop_action = screen->handle_key(screen->context, tap(KEY_BACK), START_MS);
    CHECK_EQUAL(SCREEN_ACTION_POP, pop_action.type);

    ScreenAction root_action = screen->handle_key(screen->context, hold(KEY_BACK), START_MS);
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, root_action.type);
}

static void pet_renders_in_simulator(void)
{
    Screen *screen = open_pet(NULL);
    Canvas canvas = canvas_make(screen_pixels, (Rect){0, 0, BUFFER_WIDTH, BUFFER_HEIGHT});

    memset(screen_pixels, 0, sizeof screen_pixels);
    screen->draw(screen->context, &canvas);

    int colored_pixels = 0;
    for (int index = 0; index < BUFFER_WIDTH * BUFFER_HEIGHT; index++) {
        if (screen_pixels[index] != 0) {
            colored_pixels++;
        }
    }
    CHECK(colored_pixels > 200);

    Pet *pet = pet_screen_pet(&pet_screen);
    pet->state = PET_STATE_LISTENING;
    screen->draw(screen->context, &canvas);

    pet->state = PET_STATE_SLEEPING;
    screen->draw(screen->context, &canvas);

    pet->state = PET_STATE_REACTION;
    screen->draw(screen->context, &canvas);
}

static void simulator_renders_all_four_stages(void)
{
    Screen *screen = open_pet(NULL);
    Canvas canvas = canvas_make(screen_pixels, (Rect){0, 0, BUFFER_WIDTH, BUFFER_HEIGHT});
    Pet *pet = pet_screen_pet(&pet_screen);

    PetStage stages[] = {PET_STAGE_EGG, PET_STAGE_BABY, PET_STAGE_CHILD, PET_STAGE_ADULT};
    PetState states[] = {PET_STATE_IDLE, PET_STATE_LISTENING, PET_STATE_SLEEPING,
                         PET_STATE_REACTION};

    for (size_t stage_i = 0; stage_i < sizeof stages / sizeof stages[0]; stage_i++) {
        pet->model.stage = stages[stage_i];
        for (size_t state_i = 0; state_i < sizeof states / sizeof states[0]; state_i++) {
            pet->state = states[state_i];
            memset(screen_pixels, 0, sizeof screen_pixels);
            screen->draw(screen->context, &canvas);

            int colored = 0;
            for (int i = 0; i < BUFFER_WIDTH * BUFFER_HEIGHT; i++) {
                if (screen_pixels[i] != 0) {
                    colored++;
                }
            }
            CHECK(colored > 200);
        }
    }
}

static void new_pet_starts_as_egg(void)
{
    Pet pet;
    pet_init(&pet);
    CHECK_EQUAL(PET_STAGE_EGG, pet.model.stage);
    CHECK_EQUAL(0, pet.model.listening_time_ms);
    CHECK_EQUAL(0, pet.model.affection);
    CHECK_EQUAL(0, pet.model.interaction_count);
    CHECK(strcmp("EGG", pet_stage_name(pet.model.stage)) == 0);
}

static void playback_start_transitions_to_listening(void)
{
    Pet pet;
    pet_init(&pet);
    pet_set_screen_active(&pet, true, (PlayerStatus){.state = PLAYER_IDLE}, START_MS);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(&pet));

    PlayerStatus playing = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 42,
        .position_ms = 100,
    };
    pet_update(&pet, playing, START_MS + 200);
    CHECK_EQUAL(PET_STATE_LISTENING, pet_state(&pet));
    CHECK(strcmp("LISTENING", pet_state_name(pet_state(&pet))) == 0);
}

static void pause_and_stop_handled_correctly(void)
{
    Pet pet;
    pet_init(&pet);
    pet_set_screen_active(&pet, true, (PlayerStatus){.state = PLAYER_PLAYING}, START_MS);
    CHECK_EQUAL(PET_STATE_LISTENING, pet_state(&pet));

    PlayerStatus paused = {
        .state = PLAYER_PAUSED,
        .has_track = true,
        .track = 42,
        .position_ms = 500,
    };
    pet_update(&pet, paused, START_MS + 100);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(&pet));
    CHECK(strcmp("IDLE", pet_state_name(pet_state(&pet))) == 0);

    PlayerStatus stopped = {
        .state = PLAYER_IDLE,
        .has_track = false,
        .track = 0,
        .position_ms = 0,
    };
    pet_update(&pet, stopped, START_MS + 200);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(&pet));
}

static void track_change_does_not_break_state(void)
{
    Pet pet;
    pet_init(&pet);
    PlayerStatus track1 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 10,
        .position_ms = 12000,
    };
    pet_set_screen_active(&pet, true, track1, START_MS);
    CHECK_EQUAL(PET_STATE_LISTENING, pet_state(&pet));

    PlayerStatus track2 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 11,
        .position_ms = 50,
    };
    PetAudioEvent event = pet_handle_playback(&pet, track2);
    CHECK_EQUAL(PET_AUDIO_TRACK_CHANGED, event);

    pet_update(&pet, track2, START_MS + 100);
    CHECK_EQUAL(PET_STATE_LISTENING, pet_state(&pet));
}

static void position_advance_detected(void)
{
    Pet pet;
    pet_init(&pet);
    PlayerStatus initial = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 10,
        .position_ms = 1000,
    };
    pet_handle_playback(&pet, initial);

    PlayerStatus advanced = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 10,
        .position_ms = 1500,
    };
    PetAudioEvent event = pet_handle_playback(&pet, advanced);
    CHECK_EQUAL(PET_AUDIO_POSITION_ADVANCED, event);
}

static void temporary_reaction_triggers_and_expires(void)
{
    Screen *screen = open_pet(NULL);
    Pet *pet = pet_screen_pet(&pet_screen);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(pet));
    CHECK_EQUAL(0, pet->model.interaction_count);

    ScreenAction action = screen->handle_key(screen->context, tap(KEY_PLAY), START_MS);
    CHECK_EQUAL(SCREEN_ACTION_NONE, action.type);
    CHECK_EQUAL(PET_STATE_REACTION, pet_state(pet));
    CHECK_EQUAL(1, pet->model.interaction_count);
    CHECK_EQUAL(1, pet->model.affection);
    CHECK(strcmp("HAPPY!", pet_state_name(pet_state(pet))) == 0);

    screen->update(screen->context, START_MS + PET_REACTION_DURATION_MS / 2);
    CHECK_EQUAL(PET_STATE_REACTION, pet_state(pet));

    screen->update(screen->context, START_MS + PET_REACTION_DURATION_MS + 1);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(pet));
}

static void reentering_screen_does_not_duplicate_state(void)
{
    Screen *screen = open_pet(NULL);
    Pet *pet = pet_screen_pet(&pet_screen);

    screen->handle_key(screen->context, tap(KEY_PLAY), START_MS);
    CHECK_EQUAL(PET_STATE_REACTION, pet_state(pet));

    screen->enter(screen->context, START_MS + 100);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(pet));
    CHECK_EQUAL(0, pet->reaction_until_ms);

    screen->enter(screen->context, START_MS + 200);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(pet));
}

static void screen_exit_and_sleeping_no_background_tasks(void)
{
    Screen *screen = open_pet(NULL);
    Pet *pet = pet_screen_pet(&pet_screen);

    screen->handle_key(screen->context, tap(KEY_PLAY), START_MS);
    CHECK_EQUAL(PET_STATE_REACTION, pet_state(pet));

    ScreenAction exit_action = screen->handle_key(screen->context, tap(KEY_BACK), START_MS + 100);
    CHECK_EQUAL(SCREEN_ACTION_POP, exit_action.type);
    CHECK_EQUAL(PET_STATE_SLEEPING, pet_state(pet));
    CHECK_EQUAL(0, pet->reaction_until_ms);

    screen->enter(screen->context, START_MS + 200);
    CHECK_EQUAL(PET_STATE_IDLE, pet_state(pet));
}

static void experience_accumulates_during_playback(void)
{
    Pet pet;
    pet_init(&pet);

    PlayerStatus playing1 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1000,
    };
    pet_update(&pet, playing1, 1000);
    CHECK_EQUAL(0, pet.model.listening_time_ms);

    PlayerStatus playing2 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1200,
    };
    pet_update(&pet, playing2, 1200);
    CHECK_EQUAL(200, pet.model.listening_time_ms);
}

static void experience_does_not_accumulate_while_paused_or_stopped(void)
{
    Pet pet;
    pet_init(&pet);

    PlayerStatus playing = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1000,
    };
    pet_update(&pet, playing, 1000);

    PlayerStatus playing2 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1500,
    };
    pet_update(&pet, playing2, 1500);
    CHECK_EQUAL(500, pet.model.listening_time_ms);

    PlayerStatus paused = {
        .state = PLAYER_PAUSED,
        .has_track = true,
        .track = 1,
        .position_ms = 1500,
    };
    pet_update(&pet, paused, 2000);
    pet_update(&pet, paused, 3000);
    CHECK_EQUAL(500, pet.model.listening_time_ms);

    PlayerStatus stopped = {
        .state = PLAYER_IDLE,
        .has_track = false,
        .track = 0,
        .position_ms = 0,
    };
    pet_update(&pet, stopped, 4000);
    pet_update(&pet, stopped, 5000);
    CHECK_EQUAL(500, pet.model.listening_time_ms);
}

static void seeking_does_not_generate_extra_experience(void)
{
    Pet pet;
    pet_init(&pet);

    PlayerStatus playing1 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1000,
    };
    pet_update(&pet, playing1, 1000);

    PlayerStatus jump_forward = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 35000,
    };
    pet_update(&pet, jump_forward, 1050);
    CHECK_EQUAL(50, pet.model.listening_time_ms);

    PlayerStatus jump_backward = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 2000,
    };
    pet_update(&pet, jump_backward, 1100);
    CHECK_EQUAL(50, pet.model.listening_time_ms);
}

static void track_change_preserves_accumulated_progress(void)
{
    Pet pet;
    pet_init(&pet);

    PlayerStatus t1 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1000,
    };
    pet_update(&pet, t1, 1000);

    PlayerStatus t1_adv = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 1300,
    };
    pet_update(&pet, t1_adv, 1300);
    CHECK_EQUAL(300, pet.model.listening_time_ms);

    PlayerStatus t2 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 2,
        .position_ms = 0,
    };
    pet_update(&pet, t2, 1350);

    PlayerStatus t2_adv = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 2,
        .position_ms = 200,
    };
    pet_update(&pet, t2_adv, 1550);
    CHECK_EQUAL(550, pet.model.listening_time_ms);
}

static void progress_accumulates_when_screen_is_closed(void)
{
    Pet pet;
    pet_init(&pet);
    pet.screen_active = false;

    PlayerStatus p1 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 0,
    };
    pet_update(&pet, p1, 1000);

    PlayerStatus p2 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 600,
    };
    pet_update(&pet, p2, 1600);
    CHECK_EQUAL(600, pet.model.listening_time_ms);
    CHECK_EQUAL(PET_STATE_SLEEPING, pet_state(&pet));

    pet_set_screen_active(&pet, true, p2, 1600);
    CHECK_EQUAL(PET_STATE_LISTENING, pet_state(&pet));
    CHECK_EQUAL(600, pet.model.listening_time_ms);
}

static void stage_transitions_at_each_threshold(void)
{
    Pet pet;
    pet_init(&pet);
    CHECK_EQUAL(PET_STAGE_EGG, pet.model.stage);

    pet.model.listening_time_ms = PET_STAGE_BABY_THRESHOLD_MS - 50;
    PlayerStatus p1 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 100,
    };
    pet_update(&pet, p1, 1000);

    PlayerStatus p2 = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 200,
    };
    pet_update(&pet, p2, 1100);
    CHECK_EQUAL(PET_STAGE_BABY, pet.model.stage);

    pet.model.listening_time_ms = PET_STAGE_CHILD_THRESHOLD_MS - 50;
    pet.was_playing = false;
    pet_update(&pet, p1, 2000);
    pet_update(&pet, p2, 2100);
    CHECK_EQUAL(PET_STAGE_CHILD, pet.model.stage);

    pet.model.listening_time_ms = PET_STAGE_ADULT_THRESHOLD_MS - 50;
    pet.was_playing = false;
    pet_update(&pet, p1, 3000);
    pet_update(&pet, p2, 3100);
    CHECK_EQUAL(PET_STAGE_ADULT, pet.model.stage);
}

static void large_progress_restores_correct_stage(void)
{
    Pet pet;
    pet_init(&pet);
    pet.model.listening_time_ms = 50000000u;
    pet.model.stage = PET_STAGE_EGG;

    PetStage appropriate = pet_stage_for_time(pet.model.listening_time_ms);
    CHECK_EQUAL(PET_STAGE_ADULT, appropriate);

    pet_save(&pet);
    Pet restored;
    pet_init(&restored);
    CHECK(pet_load(&restored));
    CHECK_EQUAL(PET_STAGE_ADULT, restored.model.stage);
    CHECK_EQUAL(50000000u, restored.model.listening_time_ms);
}

static void save_and_load_roundtrip(void)
{
    fake_hal_reset();
    Pet pet;
    pet_init(&pet);
    pet.model.stage = PET_STAGE_CHILD;
    pet.model.listening_time_ms = 12345678u;
    pet.model.affection = 75;
    pet.model.interaction_count = 89;

    CHECK(pet_save(&pet));

    Pet loaded;
    pet_init(&loaded);
    CHECK(pet_load(&loaded));

    CHECK_EQUAL(PET_STAGE_CHILD, loaded.model.stage);
    CHECK_EQUAL(12345678u, loaded.model.listening_time_ms);
    CHECK_EQUAL(75, loaded.model.affection);
    CHECK_EQUAL(89, loaded.model.interaction_count);
}

static void corrupted_or_missing_save_handled_safely(void)
{
    fake_hal_reset();
    Pet pet;
    pet_init(&pet);
    CHECK(!pet_load(&pet));
    CHECK_EQUAL(PET_STAGE_EGG, pet.model.stage);
    CHECK_EQUAL(0, pet.model.listening_time_ms);

    uint8_t garbage[PET_STATE_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    hal_file_write(PET_STORE_PATH, garbage, sizeof garbage);

    CHECK(!pet_load(&pet));
    CHECK_EQUAL(PET_STAGE_EGG, pet.model.stage);
    CHECK_EQUAL(0, pet.model.listening_time_ms);
}

static void restarting_does_not_duplicate_experience(void)
{
    fake_hal_reset();
    Pet pet;
    pet_init(&pet);
    pet.model.listening_time_ms = 10000;
    pet_save(&pet);

    Pet restarted;
    pet_init(&restarted);
    pet_load(&restarted);
    CHECK_EQUAL(10000, restarted.model.listening_time_ms);
    CHECK(!restarted.was_playing);

    PlayerStatus playing = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 5000,
    };
    pet_update(&restarted, playing, 5000);
    CHECK_EQUAL(10000, restarted.model.listening_time_ms);
}

static void progress_percent_and_formatting(void)
{
    PetModel egg_start = {.stage = PET_STAGE_EGG, .listening_time_ms = 0};
    CHECK_EQUAL(0, pet_stage_progress_percent(&egg_start));

    PetModel egg_half = {.stage = PET_STAGE_EGG,
                         .listening_time_ms = PET_STAGE_BABY_THRESHOLD_MS / 2};
    CHECK_EQUAL(50, pet_stage_progress_percent(&egg_half));

    PetModel adult = {.stage = PET_STAGE_ADULT, .listening_time_ms = 99999999u};
    CHECK_EQUAL(100, pet_stage_progress_percent(&adult));

    char text[32];
    pet_format_duration(45000, text, sizeof text);
    CHECK(strcmp("45s", text) == 0);

    pet_format_duration(150000, text, sizeof text);
    CHECK(strcmp("2m 30s", text) == 0);

    pet_format_duration(3665000, text, sizeof text);
    CHECK(strcmp("1h 1m", text) == 0);
}

static void model_encode_decode_roundtrip(void)
{
    PetModel original = {
        .version = PET_FORMAT_VERSION,
        .stage = PET_STAGE_CHILD,
        .listening_time_ms = 987654,
        .affection = 85,
        .interaction_count = 123,
    };
    uint8_t bytes[PET_STATE_SIZE];
    PetModel decoded;

    pet_model_encode(&original, bytes);
    CHECK(pet_model_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(original.version, decoded.version);
    CHECK_EQUAL(original.stage, decoded.stage);
    CHECK_EQUAL(original.listening_time_ms, decoded.listening_time_ms);
    CHECK_EQUAL(original.affection, decoded.affection);
    CHECK_EQUAL(original.interaction_count, decoded.interaction_count);

    bytes[2] ^= 0xFF;
    CHECK(!pet_model_decode(bytes, sizeof bytes, &decoded));
}

static void audio_critical_path_isolated(void)
{
    Pet pet;
    pet_init(&pet);
    PlayerStatus status = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 5,
        .position_ms = 4000,
        .duration_ms = 180000,
        .volume = 20,
    };
    PlayerStatus copy = status;

    pet_handle_playback(&pet, status);
    pet_update(&pet, status, START_MS);

    CHECK_EQUAL(copy.state, status.state);
    CHECK_EQUAL(copy.has_track, status.has_track);
    CHECK_EQUAL(copy.track, status.track);
    CHECK_EQUAL(copy.position_ms, status.position_ms);
    CHECK_EQUAL(copy.duration_ms, status.duration_ms);
    CHECK_EQUAL(copy.volume, status.volume);
}

static void pet_survives_restart_and_continues_accumulating_experience(void)
{
    fake_hal_reset();
    Pet pet;
    pet_init(&pet);

    PlayerStatus playing = {
        .state = PLAYER_PLAYING,
        .has_track = true,
        .track = 1,
        .position_ms = 0,
    };
    pet_update(&pet, playing, 1000);

    for (int i = 1; i <= 5; i++) {
        playing.position_ms = (uint32_t)(i * 1000);
        pet_update(&pet, playing, (uint32_t)(1000 + i * 1000));
    }
    CHECK_EQUAL(5000, pet.model.listening_time_ms);
    CHECK(pet.dirty);

    CHECK(pet_save(&pet));
    CHECK(!pet.dirty);

    pet_init(&pet);
    CHECK_EQUAL(0, pet.model.listening_time_ms);

    Pet reloaded;
    pet_init(&reloaded);
    CHECK(pet_load(&reloaded));
    CHECK_EQUAL(5000, reloaded.model.listening_time_ms);
    CHECK_EQUAL(PET_STAGE_EGG, reloaded.model.stage);
    CHECK(!reloaded.was_playing);

    playing.position_ms = 5000;
    pet_update(&reloaded, playing, 10000);
    CHECK_EQUAL(5000, reloaded.model.listening_time_ms);

    for (int i = 1; i <= 3; i++) {
        playing.position_ms = (uint32_t)(5000 + i * 1000);
        pet_update(&reloaded, playing, (uint32_t)(10000 + i * 1000));
    }
    CHECK_EQUAL(8000, reloaded.model.listening_time_ms);
}

static void pet_save_auto_reserves_if_file_missing(void)
{
    fake_hal_reset();
    Pet pet;
    pet_init(&pet);
    pet.model.listening_time_ms = 2500000;
    pet.model.stage = PET_STAGE_BABY;
    pet.dirty = true;

    CHECK(pet_save(&pet));
    CHECK(!pet.dirty);

    Pet loaded;
    pet_init(&loaded);
    CHECK(pet_load(&loaded));
    CHECK_EQUAL(2500000, loaded.model.listening_time_ms);
    CHECK_EQUAL(PET_STAGE_BABY, loaded.model.stage);
}

static void pet_periodic_save_skips_write_when_not_dirty(void)
{
    fake_hal_reset();
    Pet pet;
    pet_init(&pet);
    pet.dirty = false;

    int writes_before = fake_hal_write_count();
    CHECK(!pet_save_periodically(&pet, 40000));
    CHECK_EQUAL(writes_before, fake_hal_write_count());

    pet.dirty = true;
    CHECK(pet_save_periodically(&pet, 40000));
    CHECK_EQUAL(writes_before + 1, fake_hal_write_count());
}

static void pet_load_rejects_incompatible_version(void)
{
    fake_hal_reset();
    PetModel model = {
        .version = 99,
        .stage = PET_STAGE_BABY,
        .listening_time_ms = 500000,
    };
    uint8_t bytes[PET_STATE_SIZE];
    pet_model_encode(&model, bytes);
    hal_file_write(PET_STORE_PATH, bytes, sizeof bytes);

    Pet pet;
    pet_init(&pet);
    CHECK(!pet_load(&pet));
    CHECK_EQUAL(PET_STAGE_EGG, pet.model.stage);
    CHECK_EQUAL(0, pet.model.listening_time_ms);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(screen_opens_and_exits_cleanly),
        TEST_CASE(pet_renders_in_simulator),
        TEST_CASE(simulator_renders_all_four_stages),
        TEST_CASE(new_pet_starts_as_egg),
        TEST_CASE(playback_start_transitions_to_listening),
        TEST_CASE(pause_and_stop_handled_correctly),
        TEST_CASE(track_change_does_not_break_state),
        TEST_CASE(position_advance_detected),
        TEST_CASE(temporary_reaction_triggers_and_expires),
        TEST_CASE(reentering_screen_does_not_duplicate_state),
        TEST_CASE(screen_exit_and_sleeping_no_background_tasks),
        TEST_CASE(experience_accumulates_during_playback),
        TEST_CASE(experience_does_not_accumulate_while_paused_or_stopped),
        TEST_CASE(seeking_does_not_generate_extra_experience),
        TEST_CASE(track_change_preserves_accumulated_progress),
        TEST_CASE(progress_accumulates_when_screen_is_closed),
        TEST_CASE(stage_transitions_at_each_threshold),
        TEST_CASE(large_progress_restores_correct_stage),
        TEST_CASE(save_and_load_roundtrip),
        TEST_CASE(corrupted_or_missing_save_handled_safely),
        TEST_CASE(restarting_does_not_duplicate_experience),
        TEST_CASE(progress_percent_and_formatting),
        TEST_CASE(model_encode_decode_roundtrip),
        TEST_CASE(audio_critical_path_isolated),
        TEST_CASE(pet_survives_restart_and_continues_accumulating_experience),
        TEST_CASE(pet_save_auto_reserves_if_file_missing),
        TEST_CASE(pet_periodic_save_skips_write_when_not_dirty),
        TEST_CASE(pet_load_rejects_incompatible_version),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
