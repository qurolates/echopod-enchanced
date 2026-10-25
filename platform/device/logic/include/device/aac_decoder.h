#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "aacdec.h"
#include "codec/flac.h"

enum {
    AAC_INPUT_BYTES = 4096,
    AAC_OUTPUT_BITS = 24,
    AAC_FRAME_SAMPLES = 1024,
    AAC_STATE_BYTES = 21504
};

typedef enum AacResult {
    AAC_DECODED,
    AAC_STARVED,
    AAC_FINISHED,
    AAC_FAILED
} AacResult;

typedef struct AacDecoder {
    HAACDecoder helix;
    uint8_t state_pool[AAC_STATE_BYTES];
    unsigned char input[AAC_INPUT_BYTES];
    short pcm_interleaved[AAC_MAX_NCHANS * AAC_MAX_NSAMPS];
    int32_t channel_samples[AAC_MAX_NCHANS][AAC_MAX_NSAMPS];
    FlacInput source;
    uint32_t buffer_start;
    uint32_t buffered;
    uint32_t consumed_in_buffer;
    uint32_t decoded_samples;
    uint8_t channels;
    bool is_raw;
    bool refill;
    bool finished;
} AacDecoder;

void aac_decoder_start(AacDecoder *decoder, FlacInput source, bool is_raw, uint8_t channels,
                       uint32_t sample_rate);
AacResult aac_decode(AacDecoder *decoder);
uint32_t aac_decoder_position(const AacDecoder *decoder);
uint32_t aac_decoder_samples(const AacDecoder *decoder);
uint8_t aac_decoder_channels(const AacDecoder *decoder);
const int32_t *aac_decoder_channel(const AacDecoder *decoder, int channel);
