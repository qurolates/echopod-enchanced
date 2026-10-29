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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(screen_opens_and_exits_cleanly),
        TEST_CASE(pet_renders_in_simulator),
        TEST_CASE(playback_start_transitions_to_listening),
        TEST_CASE(pause_and_stop_handled_correctly),
        TEST_CASE(track_change_does_not_break_state),
        TEST_CASE(position_advance_detected),
        TEST_CASE(temporary_reaction_triggers_and_expires),
        TEST_CASE(reentering_screen_does_not_duplicate_state),
        TEST_CASE(screen_exit_and_sleeping_no_background_tasks),
        TEST_CASE(model_encode_decode_roundtrip),
        TEST_CASE(audio_critical_path_isolated),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
