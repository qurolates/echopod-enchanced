#include "device/core1_aac.h"

static void start_aac(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                      const bool *starved)
{
    Core1Aac *aac = context;
    (void)starved;
    aac->next_sample = first_sample;
    aac_decoder_start(aac->decoder, input, aac->is_raw, (uint8_t)info.channels, info.sample_rate);
}

static Core1DecodeResult decode_aac(void *context, Core1Block *block)
{
    Core1Aac *aac = context;
    switch (aac_decode(aac->decoder)) {
    case AAC_DECODED:
        break;
    case AAC_STARVED:
        return CORE1_STARVED;
    case AAC_FINISHED:
        return CORE1_FINISHED;
    case AAC_FAILED:
        return CORE1_FAILED;
    }
    uint8_t channels = aac_decoder_channels(aac->decoder);
    block->channels[0] = aac_decoder_channel(aac->decoder, 0);
    block->channels[1] = aac_decoder_channel(aac->decoder, channels > 1 ? 1 : 0);
    block->first_sample = aac->next_sample;
    block->samples = aac_decoder_samples(aac->decoder);
    block->position = aac_decoder_position(aac->decoder);
    aac->next_sample += block->samples;
    return CORE1_DECODED;
}

Core1Decoder core1_aac_decoder(Core1Aac *aac, AacDecoder *decoder, bool is_raw)
{
    *aac = (Core1Aac){.decoder = decoder, .next_sample = 0, .is_raw = is_raw};
    return (Core1Decoder){start_aac, decode_aac, aac};
}
