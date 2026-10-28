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
    CHECK_EQUAL(4, eq_screen.model.count(eq_screen.model.context));
    CHECK(strcmp("Equalizer", eq_screen.model.label(eq_screen.model.context, EQ_ROW_ENABLED)) == 0);
    CHECK(strcmp("Off", eq_screen.model.detail(eq_screen.model.context, EQ_ROW_ENABLED)) == 0);
    CHECK(strcmp("Preset", eq_screen.model.label(eq_screen.model.context, EQ_ROW_PRESET)) == 0);
    CHECK(strcmp("Flat", eq_screen.model.detail(eq_screen.model.context, EQ_ROW_PRESET)) == 0);
    CHECK(strcmp("Custom EQ", eq_screen.model.label(eq_screen.model.context, EQ_ROW_CUSTOM_EQ)) == 0);
    CHECK(strcmp("Reset Equalizer", eq_screen.model.label(eq_screen.model.context, EQ_ROW_RESET)) == 0);
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
    CHECK(strcmp("On", eq_screen.model.detail(eq_screen.model.context, EQ_ROW_ENABLED)) == 0);
    action = screen->handle_key(screen->context, play, 20);
    CHECK(!store.settings.eq_enabled);
    CHECK(!fake_hal_audio_eq_enabled());
}

static void custom_eq_manual_adjustment_updates_dsp_and_sets_custom(void)
{
    setup_screen();
    eq_screen.list.selected = EQ_ROW_CUSTOM_EQ;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    Screen *custom_screen = action.target;
    CHECK(custom_screen != NULL);
    custom_screen->enter(custom_screen->context, 20);

    KeyEvent next_band = {.key = KEY_VOLUME_UP, .gesture = KEY_GESTURE_PRESS};
    custom_screen->handle_key(custom_screen->context, next_band, 30);
    CHECK_EQUAL(1, eq_screen.custom_eq.selected_band);

    custom_screen->handle_key(custom_screen->context, play, 40);
    CHECK(eq_screen.custom_eq.adjusting);

    KeyEvent gain_up = {.key = KEY_VOLUME_UP, .gesture = KEY_GESTURE_PRESS};
    custom_screen->handle_key(custom_screen->context, gain_up, 50);
    custom_screen->handle_key(custom_screen->context, gain_up, 60);
    CHECK_EQUAL(2, store.settings.eq_gains[1]);
    CHECK_EQUAL(EQ_PRESET_CUSTOM, store.settings.eq_preset);
    CHECK_EQUAL(2, fake_hal_audio_eq_gains()[1]);

    custom_screen->handle_key(custom_screen->context, play, 70);
    CHECK(!eq_screen.custom_eq.adjusting);

    KeyEvent back = {.key = KEY_BACK, .gesture = KEY_GESTURE_PRESS};
    action = custom_screen->handle_key(custom_screen->context, back, 80);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);

    screen->enter(screen->context, 90);
    CHECK_EQUAL(5, eq_screen.model.count(eq_screen.model.context));
    CHECK(strcmp("Save Preset", eq_screen.model.label(eq_screen.model.context, EQ_ROW_SAVE_PRESET)) == 0);
    CHECK(strcmp("Custom", eq_screen.model.detail(eq_screen.model.context, EQ_ROW_PRESET)) == 0);
}

static void save_preset_saves_user_preset_and_updates_preset_list(void)
{
    setup_screen();
    store.settings.eq_preset = EQ_PRESET_CUSTOM;
    store.settings.eq_gains[0] = 3;
    store.settings.eq_gains[1] = 2;

    eq_screen.list.selected = EQ_ROW_SAVE_PRESET;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    Screen *save_scr = action.target;
    CHECK(save_scr != NULL);
    save_scr->enter(save_scr->context, 20);

    action = save_scr->handle_key(save_scr->context, play, 30);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(1, store.settings.eq_user_preset_count);
    CHECK(strcmp("Preset 1", store.settings.eq_user_presets[0].name) == 0);
    CHECK_EQUAL(store.settings.eq_user_presets[0].id, store.settings.eq_preset);

    eq_screen.list.selected = EQ_ROW_PRESET;
    action = screen->handle_key(screen->context, play, 40);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    Screen *presets_scr = action.target;
    presets_scr->enter(presets_scr->context, 50);

    int total_presets = eq_screen.preset_model.count(eq_screen.preset_model.context);
    CHECK_EQUAL(EQ_PRESET_COUNT + 1, total_presets);
    CHECK(strcmp("Preset 1", eq_screen.preset_model.label(eq_screen.preset_model.context, EQ_PRESET_COUNT)) == 0);
    CHECK(eq_screen.preset_model.detail(eq_screen.preset_model.context, EQ_PRESET_COUNT) != NULL);
}

static void preset_choice_loads_preset_gains_and_applies(void)
{
    setup_screen();
    eq_screen.list.selected = EQ_ROW_PRESET;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    Screen *presets_scr = action.target;
    presets_scr->enter(presets_scr->context, 20);

    KeyEvent down = {.key = KEY_VOLUME_UP, .gesture = KEY_GESTURE_PRESS};
    for (int step = 0; step < 5; step++) {
        presets_scr->handle_key(presets_scr->context, down, 30u + (uint32_t)step);
    }
    action = presets_scr->handle_key(presets_scr->context, play, 50);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(5, store.settings.eq_preset);
    const EqPreset *rock = eq_preset(5);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK_EQUAL(rock->gains[band], store.settings.eq_gains[band]);
        CHECK_EQUAL(rock->gains[band], fake_hal_audio_eq_gains()[band]);
    }
}

static void reset_equalizer_dialog_clears_gains_to_flat(void)
{
    setup_screen();
    store.settings.eq_gains[0] = 5;
    store.settings.eq_gains[1] = 3;
    store.settings.eq_preset = EQ_PRESET_CUSTOM;
    int8_t user_gains[5] = {1, 1, 1, 1, 1};
    settings_save_user_preset(&store.settings, "My EQ", user_gains);
    CHECK_EQUAL(1, store.settings.eq_user_preset_count);

    eq_screen.list.selected = EQ_ROW_RESET;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    Screen *reset_scr = action.target;
    reset_scr->enter(reset_scr->context, 20);

    eq_screen.reset_choice.config.choose(&eq_screen, 1);
    CHECK_EQUAL(0, store.settings.eq_preset);
    for (int band = 0; band < EQ_BAND_COUNT; band++) {
        CHECK_EQUAL(0, store.settings.eq_gains[band]);
        CHECK_EQUAL(0, fake_hal_audio_eq_gains()[band]);
    }
    CHECK_EQUAL(1, store.settings.eq_user_preset_count);
}

static void delete_user_preset_via_hold(void)
{
    setup_screen();
    int8_t gains[5] = {2, 2, 2, 2, 2};
    settings_save_user_preset(&store.settings, "To Delete", gains);
    CHECK_EQUAL(1, store.settings.eq_user_preset_count);

    eq_screen.list.selected = EQ_ROW_PRESET;
    KeyEvent play = {.key = KEY_PLAY, .gesture = KEY_GESTURE_PRESS};
    ScreenAction action = screen->handle_key(screen->context, play, 10);
    Screen *presets_scr = action.target;
    presets_scr->enter(presets_scr->context, 20);

    eq_screen.preset_list.selected = EQ_PRESET_COUNT;
    KeyEvent hold = {.key = KEY_PLAY, .gesture = KEY_GESTURE_LONG_PRESS};
    action = presets_scr->handle_key(presets_scr->context, hold, 30);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    Screen *del_scr = action.target;
    CHECK(del_scr != NULL);
    del_scr->enter(del_scr->context, 40);

    eq_screen.delete_choice.config.choose(&eq_screen, 1);
    CHECK_EQUAL(0, store.settings.eq_user_preset_count);
    CHECK_EQUAL(EQ_PRESET_CUSTOM, store.settings.eq_preset);
}

static void draw_renders_without_crash(void)
{
    setup_screen();
    Color pixels[320 * 170] = {0};
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, 320, 170});
    screen->draw(screen->context, &canvas);

    eq_screen.custom_eq.screen.draw(&eq_screen.custom_eq, &canvas);
    eq_screen.save_preset.screen.draw(&eq_screen.save_preset, &canvas);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(default_rows_and_labels),
        TEST_CASE(toggling_equalizer_switches_state),
        TEST_CASE(custom_eq_manual_adjustment_updates_dsp_and_sets_custom),
        TEST_CASE(save_preset_saves_user_preset_and_updates_preset_list),
        TEST_CASE(preset_choice_loads_preset_gains_and_applies),
        TEST_CASE(reset_equalizer_dialog_clears_gains_to_flat),
        TEST_CASE(delete_user_preset_via_hold),
        TEST_CASE(draw_renders_without_crash),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
