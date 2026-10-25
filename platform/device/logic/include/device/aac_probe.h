#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    AAC_SIGNATURE_BYTES = 12
};

typedef struct AacStream {
    uint32_t sample_rate;
    uint8_t channels;
    uint16_t samples_per_frame;
    uint32_t total_samples;
    uint32_t audio_start;
    uint32_t audio_end;
    uint32_t stco_offset;
    uint32_t chunk_count;
    bool is_mp4;
    bool is_co64;
} AacStream;

bool aac_signature(const uint8_t *bytes, uint32_t length);
bool aac_probe(FlacInput input, AacStream *stream);
uint32_t aac_seek_offset(FlacInput input, const AacStream *stream, uint32_t sample,
                         uint32_t *actual_sample);
