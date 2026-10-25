#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/core1_stream.h"
#include "device/opus_decoder.h"

typedef struct Core1Opus {
    OpusDecoderContext *decoder;
    uint32_t next_sample;
} Core1Opus;

Core1Decoder core1_opus_decoder(Core1Opus *opus, OpusDecoderContext *decoder);
