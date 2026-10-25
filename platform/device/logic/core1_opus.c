#include "device/core1_opus.h"

static void start_opus(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                       const bool *starved)
{
    Core1Opus *opus = context;
    (void)starved;
    opus->next_sample = first_sample;
    opus_decoder_start(opus->decoder, input, (uint8_t)info.channels, info.sample_rate);
}

static Core1DecodeResult decode_opus(void *context, Core1Block *block)
{
    Core1Opus *opus = context;
    switch (opus_decoder_step(opus->decoder)) {
    case OPUS_DECODED:
        break;
    case OPUS_STARVED:
        return CORE1_STARVED;
    case OPUS_FINISHED:
        return CORE1_FINISHED;
    case OPUS_FAILED:
        return CORE1_FAILED;
    }
    uint8_t channels = opus_decoder_channels(opus->decoder);
    block->channels[0] = opus_decoder_channel(opus->decoder, 0);
    block->channels[1] = opus_decoder_channel(opus->decoder, channels > 1 ? 1 : 0);
    block->first_sample = opus->next_sample;
    block->samples = opus_decoder_samples(opus->decoder);
    block->position = opus_decoder_position(opus->decoder);
    opus->next_sample += block->samples;
    return CORE1_DECODED;
}

Core1Decoder core1_opus_decoder(Core1Opus *opus, OpusDecoderContext *decoder)
{
    *opus = (Core1Opus){.decoder = decoder, .next_sample = 0};
    return (Core1Decoder){start_opus, decode_opus, opus};
}
