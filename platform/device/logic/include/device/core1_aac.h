#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/aac_decoder.h"
#include "device/core1_stream.h"

typedef struct Core1Aac {
    AacDecoder *decoder;
    uint32_t next_sample;
    bool is_raw;
} Core1Aac;

Core1Decoder core1_aac_decoder(Core1Aac *aac, AacDecoder *decoder, bool is_raw);
