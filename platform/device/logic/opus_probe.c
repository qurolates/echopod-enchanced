#include "device/opus_probe.h"

#include <stddef.h>

enum {
    SIGNATURE_BYTES = 36,
    OGG_TAIL_BYTES = 1024,
    SECTOR_BYTES = 512
};

static uint64_t read_le64(const uint8_t *bytes)
{
    uint64_t value = 0;
    for (int index = 0; index < 8; index++) {
        value |= (uint64_t)bytes[index] << (index * 8);
    }
    return value;
}

bool opus_signature(const uint8_t *bytes, uint32_t length)
{
    if (length < SIGNATURE_BYTES) {
        return false;
    }
    return bytes[0] == 'O' && bytes[1] == 'g' && bytes[2] == 'g' && bytes[3] == 'S' &&
           bytes[4] == 0 && bytes[26] >= 1 &&
           bytes[28] == 'O' && bytes[29] == 'p' && bytes[30] == 'u' && bytes[31] == 's' &&
           bytes[32] == 'H' && bytes[33] == 'e' && bytes[34] == 'a' && bytes[35] == 'd';
}

static bool find_last_granule(FlacInput input, uint64_t *granule)
{
    uint8_t buf[OGG_TAIL_BYTES];
    uint32_t len = input.size < sizeof buf ? input.size : (uint32_t)sizeof buf;
    uint32_t pos = input.size - len;
    if (!input.read(input.context, pos, buf, len)) {
        return false;
    }
    for (uint32_t i = len >= 14 ? len - 14 : 0; i-- > 0;) {
        if (buf[i] == 'O' && buf[i + 1] == 'g' && buf[i + 2] == 'g' && buf[i + 3] == 'S') {
            *granule = read_le64(buf + i + 6);
            return true;
        }
    }
    return false;
}

bool opus_probe(FlacInput input, OpusStream *stream)
{
    uint8_t header[64];
    if (input.size < SIGNATURE_BYTES || !input.read(input.context, 0, header, sizeof header)) {
        return false;
    }
    if (!opus_signature(header, sizeof header)) {
        return false;
    }
    uint8_t channels = header[28 + 9];
    if (channels == 0 || channels > 2) {
        return false;
    }
    stream->sample_rate = 48000;
    stream->channels = channels;
    stream->pre_skip = (uint16_t)((uint16_t)header[28 + 10] | ((uint16_t)header[28 + 11] << 8));
    stream->audio_start = 0;
    stream->audio_end = input.size;
    stream->total_samples = 0;

    uint64_t granule = 0;
    if (find_last_granule(input, &granule) && granule != UINT64_MAX) {
        if (granule > stream->pre_skip) {
            stream->total_samples = (uint32_t)(granule - stream->pre_skip);
        }
    }
    return true;
}

uint32_t opus_seek_offset(FlacInput input, const OpusStream *stream, uint32_t target_sample,
                          uint32_t *actual_sample)
{
    *actual_sample = target_sample;
    if (stream->total_samples == 0 || target_sample == 0) {
        return 0;
    }
    if (target_sample >= stream->total_samples) {
        return input.size;
    }
    uint64_t offset = (uint64_t)target_sample * (uint64_t)input.size / (uint64_t)stream->total_samples;
    return (uint32_t)((offset / SECTOR_BYTES) * SECTOR_BYTES);
}
