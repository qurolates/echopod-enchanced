#include "diagnostics_rows.h"

#include "core/string_builder.h"
#include "diagnostics_values.h"
#include "hal/audio.h"
#include "hal/power.h"
#include "hal/system.h"

const char *diagnostics_row_firmware(const void *context)
{
    (void)context;
    return hal_system_version();
}

static char battery_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_battery(const void *context)
{
    (void)context;
    diagnostics_battery_text(hal_power_battery(), battery_text, sizeof battery_text);
    return battery_text;
}

static char jack_single_ended_text[DIAGNOSTICS_VALUE_CAPACITY];
static char jack_balanced_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_jack_single_ended(const void *context)
{
    (void)context;
    diagnostics_jack_pin_text(hal_audio_jack_pins().single_ended, jack_single_ended_text,
                              sizeof jack_single_ended_text);
    return jack_single_ended_text;
}

const char *diagnostics_row_jack_balanced(const void *context)
{
    (void)context;
    diagnostics_jack_pin_text(hal_audio_jack_pins().balanced, jack_balanced_text,
                              sizeof jack_balanced_text);
    return jack_balanced_text;
}

static char start_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_start(const void *context)
{
    (void)context;
    diagnostics_start_text(hal_system_start_info(), start_text, sizeof start_text);
    return start_text;
}

static char last_step_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_last_step(const void *context)
{
    (void)context;
    diagnostics_last_step_text(hal_system_start_info(), last_step_text, sizeof last_step_text);
    return last_step_text;
}

static char last_battery_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_last_battery(const void *context)
{
    (void)context;
    diagnostics_last_battery_text(hal_system_start_info(), last_battery_text,
                                  sizeof last_battery_text);
    return last_battery_text;
}

static char usb_last_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_usb_last(const void *context)
{
    (void)context;
    diagnostics_usb_last_text(hal_system_usb_last(), usb_last_text, sizeof usb_last_text);
    return usb_last_text;
}

static char audio_state_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_audio_state(const void *context)
{
    (void)context;
    HalAudioStatus status = hal_audio_status();
    const char *state_str = "Stop";
    if (status.state == HAL_AUDIO_PLAYING) {
        state_str = "Play";
    } else if (status.state == HAL_AUDIO_PAUSED) {
        state_str = "Pause";
    }
    const char *codec_str = "FLAC";
    switch (status.format.codec) {
    case HAL_AUDIO_CODEC_MP3:
        codec_str = "MP3";
        break;
    case HAL_AUDIO_CODEC_WAV:
        codec_str = "WAV";
        break;
    case HAL_AUDIO_CODEC_AAC:
        codec_str = "AAC";
        break;
    case HAL_AUDIO_CODEC_OPUS:
        codec_str = "OPUS";
        break;
    case HAL_AUDIO_CODEC_UNKNOWN:
        codec_str = "UNK";
        break;
    default:
        break;
    }
    StringBuilder sb = string_builder_make(audio_state_text, sizeof audio_state_text);
    string_builder_append(&sb, state_str);
    string_builder_append_char(&sb, ' ');
    string_builder_append(&sb, codec_str);
    string_builder_append_char(&sb, ' ');
    string_builder_append_unsigned(&sb, status.position_ms / 1000u);
    string_builder_append_char(&sb, 's');
    return audio_state_text;
}

static char audio_buffer_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_audio_buffer(const void *context)
{
    (void)context;
    HalAudioStats stats = hal_audio_stats();
    StringBuilder sb = string_builder_make(audio_buffer_text, sizeof audio_buffer_text);
    string_builder_append_unsigned(&sb, stats.buffered_ms);
    string_builder_append(&sb, "ms U:");
    string_builder_append_unsigned(&sb, stats.underruns);
    string_builder_append(&sb, " OU:");
    string_builder_append_unsigned(&sb, stats.output_underruns);
    return audio_buffer_text;
}

static char audio_pipeline_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_audio_pipeline(const void *context)
{
    (void)context;
    HalAudioStats stats = hal_audio_stats();
    StringBuilder sb = string_builder_make(audio_pipeline_text, sizeof audio_pipeline_text);
    string_builder_append(&sb, "P:");
    string_builder_append_unsigned(&sb, stats.produced);
    string_builder_append(&sb, " C:");
    string_builder_append_unsigned(&sb, stats.consumed);
    string_builder_append(&sb, " B:");
    string_builder_append_unsigned(&sb, stats.blocks);
    return audio_pipeline_text;
}

static char dma_state_text[DIAGNOSTICS_VALUE_CAPACITY];

static void append_dma_hex(StringBuilder *builder, uint32_t value, int digits)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int digit = digits - 1; digit >= 0; digit--) {
        string_builder_append_char(builder, DIGITS[(value >> (4 * digit)) & 0xFu]);
    }
}

const char *diagnostics_row_dma_state(const void *context)
{
    (void)context;
    HalAudioStats stats = hal_audio_stats();
    StringBuilder sb = string_builder_make(dma_state_text, sizeof dma_state_text);
    string_builder_append(&sb, stats.dma_running ? "Run " : "Stop ");
    string_builder_append(&sb, stats.transferring ? "Xfer " : "Idle ");
    string_builder_append(&sb, "0x");
    append_dma_hex(&sb, stats.dma_sar, 8);
    return dma_state_text;
}

static char dac_state_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_dac_state(const void *context)
{
    (void)context;
    HalAudioStats stats = hal_audio_stats();
    StringBuilder sb = string_builder_make(dac_state_text, sizeof dac_state_text);
    string_builder_append(&sb, "V:");
    string_builder_append_unsigned(&sb, stats.dac_volume);
    string_builder_append(&sb, stats.dac_muted ? " M:1 " : " M:0 ");
    if (stats.dac_up) {
        string_builder_append_unsigned(&sb, stats.dac_rate);
    } else {
        string_builder_append(&sb, "Down");
    }
    return dac_state_text;
}

static char dsp_state_text[DIAGNOSTICS_VALUE_CAPACITY];

const char *diagnostics_row_dsp_state(const void *context)
{
    (void)context;
    HalAudioStats stats = hal_audio_stats();
    StringBuilder sb = string_builder_make(dsp_state_text, sizeof dsp_state_text);
    string_builder_append(&sb, stats.eq_enabled ? "EQ:On " : "EQ:Off ");
    string_builder_append(&sb, stats.eq_flat ? "Flat " : "Tune ");
    string_builder_append(&sb, stats.dsp_bypass ? "BP:1" : "BP:0");
    return dsp_state_text;
}

const char *diagnostics_row_dsp_bypass(const void *context)
{
    (void)context;
    return hal_audio_dsp_bypass() ? "Bypassed" : "Active";
}
