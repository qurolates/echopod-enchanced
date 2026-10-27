#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "screens/equalizer_screen.h"

static SettingsStore store;
static EqualizerScreen eq_screen;
static Screen *screen;

static void setup_screen(void)
{
    fake_hal_reset();
    store.settings = settings_defaults();
    store.has_saved = false;
    screen = equalizer_screen_init(&eq_screen, &store, &DEFAULT_THEME);
    screen->enter(screen->context, 0);
}

static void default_rows_and_labels(void)
{
    setup_screen();
    CHECK_EQUAL(8, eq_screen.model.count(eq_screen.model.context));
    CHECK(strcmp("Equalizer", eq_screen.model.label(eq_screen.model.context, 0)) == 0);
    CHECK(strcmp("Off", eq_screen.model.detail(eq_screen.model.context, 0)) == 0);
    CHECK(strcmp("Preset", eq_screen.model.label(eq_screen.model.context, 1)) == 0);
    CHECK(strcmp("Flat", eq_screen.model.detail(eq_screen.model.context, 1)) == 0);
    CHECK(strcmp("Reset to Flat", eq_screen.model.label(eq_screen.model.context, 2)) == 0);
    CHECK(eq_screen.model.detail(eq_screen.model.context, 2) == NULL);
    CHECK(strcmp("100 Hz", eq_screen.model.label(eq_screen.model.context, 3)) == 0);
    CHECK(strcmp("0 dB", eq_screen.model.detail(eq_screen.model.context, 3)) == 0);
    CHECK(strcmp("10 kHz", eq_screen.model.label(eq_screen.model.context, 7)) == 0);
}

static void toggling_equalizer_switches_state(void)
{
    setup_screen();
    CHECK(!store.settings.eq_enabled);
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(SCREEN_ACTION_NONE, action.type);
    CHECK(store.settings.eq_enabled);
    CHECK(fake_hal_audio_eq_enabled());
    CHECK(strcmp("On", eq_screen.model.detail(eq_screen.model.context, 0)) == 0);
    action = screen->handle_key(screen->context, play, 20);
    CHECK(!store.settings.eq_enabled);
    CHECK(!fake_hal_audio_eq_enabled());
}

static void manual_adjustment_changes_gain_and_sets_custom(void)
{
    setup_screen();
    KeyEvent down = {.key = KEY_VOLUME_UP, .gesture = KEY_GESTURE_PRESS};
    for (int step = 0; step < 3; step++) {
        screen->handle_key(screen->context, down, 10u + (uint32_t)step);
    }
    CHECK_EQUAL(3, eq_screen.list.selected);

    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    screen->handle_key(screen->context, play, 50);
    CHECK(eq_screen.adjusting);

    screen->handle_key(screen->context, down, 60);
    screen->handle_key(screen->context, down, 70);
    CHECK_EQUAL(2, store.settings.eq_gains[0]);
    CHECK_EQUAL(EQ_PRESET_CUSTOM, store.settings.eq_preset);
    CHECK_EQUAL(2, fake_hal_audio_eq_gains()[0]);

    screen->handle_key(screen->context, play, 80);
    CHECK(!eq_screen.adjusting);
    CHECK(strcmp("Custom", eq_screen.model.detail(eq_screen.model.context, 1)) == 0);
}

static void reset_to_flat_clears_all_gains(void)
{
    setup_screen();
    store.settings.eq_gains[0] = 4;
    store.settings.eq_gains[1] = 2;
    store.settings.eq_preset = 5;

    eq_screen.list.selected = 2;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(0, store.settings.eq_preset);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK_EQUAL(0, store.settings.eq_gains[band]);
    }
}

static void preset_choice_loads_preset_gains(void)
{
    setup_screen();
    eq_screen.list.selected = 1;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    CHECK(action.target != NULL);

    eq_screen.preset_choice.config.choose(&eq_screen, 5);
    CHECK_EQUAL(5, store.settings.eq_preset);
    const EqPreset *rock = eq_preset(5);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK_EQUAL(rock->gains[band], store.settings.eq_gains[band]);
        CHECK_EQUAL(rock->gains[band], fake_hal_audio_eq_gains()[band]);
    }
}

static void draw_renders_without_crash(void)
{
    setup_screen();
    Color pixels[320 * 170] = {0};
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, 320, 170});
    screen->draw(screen->context, &canvas);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(default_rows_and_labels),
        TEST_CASE(toggling_equalizer_switches_state),
        TEST_CASE(manual_adjustment_changes_gain_and_sets_custom),
        TEST_CASE(reset_to_flat_clears_all_gains),
        TEST_CASE(preset_choice_loads_preset_gains),
        TEST_CASE(draw_renders_without_crash),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
