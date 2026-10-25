#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

typedef struct OpusStream {
    uint32_t sample_rate;
    uint8_t channels;
    uint16_t pre_skip;
    uint32_t total_samples;
    uint32_t audio_start;
    uint32_t audio_end;
} OpusStream;

bool opus_signature(const uint8_t *bytes, uint32_t length);
bool opus_probe(FlacInput input, OpusStream *stream);
uint32_t opus_seek_offset(FlacInput input, const OpusStream *stream, uint32_t target_sample,
                          uint32_t *actual_sample);
