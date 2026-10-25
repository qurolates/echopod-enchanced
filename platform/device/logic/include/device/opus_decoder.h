#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    OPUS_INPUT_BYTES = 1024,
    OPUS_MAX_PACKET_BYTES = 1280,
    OPUS_MAX_FRAME_SAMPLES = 960,
    OPUS_MAX_CHANNELS = 2,
    OPUS_OUTPUT_BITS = 16,
    OPUS_STATE_BYTES = 27500
};

typedef enum OpusResult {
    OPUS_DECODED,
    OPUS_STARVED,
    OPUS_FINISHED,
    OPUS_FAILED
} OpusResult;

typedef struct OpusDecoderContext {
    void *st;
    uint8_t state_pool[OPUS_STATE_BYTES];
    uint8_t input[OPUS_INPUT_BYTES];
    uint8_t packet[OPUS_MAX_PACKET_BYTES];
    int32_t channel_samples[OPUS_MAX_CHANNELS][OPUS_MAX_FRAME_SAMPLES];
    FlacInput source;
    uint32_t buffer_start;
    uint32_t buffered;
    uint32_t consumed_in_buffer;
    uint32_t packet_len;
    uint32_t decoded_samples;
    uint32_t pre_skip;
    uint32_t sync_page_offset;
    uint8_t segment_table[255];
    uint8_t segments_count;
    uint8_t segment_index;
    uint8_t channels;
    uint8_t demux_state;
    bool headers_read;
    bool refill;
    bool finished;
} OpusDecoderContext;

void opus_decoder_start(OpusDecoderContext *decoder, FlacInput source, uint8_t channels,
                        uint32_t sample_rate);
OpusResult opus_decoder_step(OpusDecoderContext *decoder);
uint32_t opus_decoder_position(const OpusDecoderContext *decoder);
uint32_t opus_decoder_samples(const OpusDecoderContext *decoder);
uint8_t opus_decoder_channels(const OpusDecoderContext *decoder);
const int32_t *opus_decoder_channel(const OpusDecoderContext *decoder, int channel);
