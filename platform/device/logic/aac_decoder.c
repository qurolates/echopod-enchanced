#include "device/aac_decoder.h"

#include <stddef.h>

enum {
    MIN_FILL_BYTES = 2048,
    PCM_SHIFT_TO_24BIT = 8
};

void aac_decoder_start(AacDecoder *decoder, FlacInput source, bool is_raw, uint8_t channels,
                       uint32_t sample_rate)
{
    void *ptr = decoder->state_pool;
    int sz = sizeof decoder->state_pool;
    decoder->helix = AACInitDecoderPre(ptr, sz);
    decoder->source = source;
    decoder->buffer_start = 0;
    decoder->buffered = 0;
    decoder->consumed_in_buffer = 0;
    decoder->decoded_samples = 0;
    decoder->channels = channels;
    decoder->is_raw = is_raw;
    if (!decoder->is_raw) {
        uint8_t magic[4] = {0};
        bool has_magic = source.size >= 4 && source.read(source.context, 0, magic, sizeof magic);
        bool is_adts = has_magic && (magic[0] == 0xFF) && ((magic[1] & 0xF6) == 0xF0);
        decoder->is_raw = !is_adts;
    }
    decoder->refill = true;
    decoder->finished = false;
    if (decoder->is_raw && decoder->helix != NULL) {
        AACFrameInfo info = {0};
        info.nChans = channels > 0 ? (int)channels : 2;
        info.sampRateCore = (int)sample_rate;
        info.profile = AAC_PROFILE_LC;
        AACSetRawBlockParams(decoder->helix, 0, &info);
    }
}

static AacResult refill(AacDecoder *decoder)
{
    if (decoder->finished) {
        return AAC_FINISHED;
    }
    uint32_t keep = decoder->buffered - decoder->consumed_in_buffer;
    for (uint32_t index = 0; index < keep; index++) {
        decoder->input[index] = decoder->input[decoder->consumed_in_buffer + index];
    }
    decoder->buffer_start += decoder->consumed_in_buffer;
    decoder->buffered = keep;
    decoder->consumed_in_buffer = 0;
    uint32_t at = decoder->buffer_start + keep;
    uint32_t left = at < decoder->source.size ? decoder->source.size - at : 0;
    uint32_t space = AAC_INPUT_BYTES - keep;
    uint32_t want = space < left ? space : left;
    if (want > 0 &&
        !decoder->source.read(decoder->source.context, at, decoder->input + keep, want)) {
        return AAC_STARVED;
    }
    decoder->buffered = keep + want;
    if (decoder->buffered == 0) {
        decoder->finished = true;
        return AAC_FINISHED;
    }
    decoder->refill = false;
    return AAC_DECODED;
}

static void convert_samples(AacDecoder *decoder, int channels, int sample_count)
{
    if (channels >= 2) {
        for (int index = 0; index < sample_count; index++) {
            decoder->channel_samples[0][index] =
                (int32_t)decoder->pcm_interleaved[index * 2] * 256;
            decoder->channel_samples[1][index] =
                (int32_t)decoder->pcm_interleaved[index * 2 + 1] * 256;
        }
    } else {
        for (int index = 0; index < sample_count; index++) {
            int32_t val = (int32_t)decoder->pcm_interleaved[index] * 256;
            decoder->channel_samples[0][index] = val;
            decoder->channel_samples[1][index] = val;
        }
    }
}

AacResult aac_decode(AacDecoder *decoder)
{
    if (decoder->helix == NULL) {
        return AAC_FAILED;
    }
    for (;;) {
        uint32_t available = decoder->buffered - decoder->consumed_in_buffer;
        if (decoder->refill || available < MIN_FILL_BYTES) {
            AacResult filled = refill(decoder);
            if (filled == AAC_STARVED) {
                return AAC_STARVED;
            }
            if (filled == AAC_FINISHED && available == 0) {
                return AAC_FINISHED;
            }
            available = decoder->buffered - decoder->consumed_in_buffer;
        }
        if (available == 0) {
            return AAC_FINISHED;
        }
        if (!decoder->is_raw) {
            unsigned char *buf = decoder->input + decoder->consumed_in_buffer;
            int sync = AACFindSyncWord(buf, (int)available);
            if (sync < 0) {
                decoder->consumed_in_buffer = decoder->buffered;
                decoder->refill = true;
                if (decoder->buffer_start + decoder->buffered >= decoder->source.size) {
                    return AAC_FINISHED;
                }
                continue;
            }
            decoder->consumed_in_buffer += (uint32_t)sync;
            available -= (uint32_t)sync;
        }
        unsigned char *inbuf = decoder->input + decoder->consumed_in_buffer;
        int bytes_left = (int)available;
        int err = AACDecode(decoder->helix, &inbuf, &bytes_left, decoder->pcm_interleaved);
        decoder->consumed_in_buffer = (uint32_t)(inbuf - decoder->input);
        if (err == 0) {
            AACFrameInfo info;
            AACGetLastFrameInfo(decoder->helix, &info);
            decoder->channels = (uint8_t)info.nChans;
            decoder->decoded_samples = (uint32_t)(info.outputSamps / (info.nChans > 0 ? info.nChans : 1));
            convert_samples(decoder, info.nChans, (int)decoder->decoded_samples);
            if (decoder->is_raw) {
                AACSetRawBlockParams(decoder->helix, 1, &info);
            }
            return AAC_DECODED;
        }
        if (err == ERR_AAC_INDATA_UNDERFLOW || err == ERR_AAC_SYNTAX_ELEMENT) {
            if (decoder->buffer_start + decoder->buffered < decoder->source.size) {
                decoder->refill = true;
                continue;
            }
            return AAC_FINISHED;
        }
        if (decoder->is_raw) {
            return AAC_FAILED;
        }
        decoder->consumed_in_buffer++;
    }
}

uint32_t aac_decoder_position(const AacDecoder *decoder)
{
    uint32_t pos = decoder->buffer_start + decoder->consumed_in_buffer;
    return pos < decoder->source.size ? pos : decoder->source.size;
}

uint32_t aac_decoder_samples(const AacDecoder *decoder)
{
    return decoder->decoded_samples;
}

uint8_t aac_decoder_channels(const AacDecoder *decoder)
{
    return decoder->channels;
}

const int32_t *aac_decoder_channel(const AacDecoder *decoder, int channel)
{
    return decoder->channel_samples[channel > 0 ? 1 : 0];
}
