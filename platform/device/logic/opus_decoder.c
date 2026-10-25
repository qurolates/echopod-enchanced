#include "device/opus_decoder.h"

#include <string.h>

#include "opus.h"

enum {
    OGG_HEADER_BYTES = 27,
    DEMUX_FIND_SYNC = 0,
    DEMUX_READ_HEADER = 1,
    DEMUX_READ_SEGMENTS = 2,
    DEMUX_READ_PACKET = 3
};

static bool refill_buffer(OpusDecoderContext *decoder)
{
    if (decoder->finished) {
        return false;
    }
    uint32_t keep = decoder->buffered - decoder->consumed_in_buffer;
    if (keep > 0 && decoder->consumed_in_buffer > 0) {
        memmove(decoder->input, decoder->input + decoder->consumed_in_buffer, keep);
    }
    decoder->buffer_start += decoder->consumed_in_buffer;
    decoder->buffered = keep;
    decoder->consumed_in_buffer = 0;

    uint32_t at = decoder->buffer_start + keep;
    uint32_t left = at < decoder->source.size ? decoder->source.size - at : 0;
    uint32_t space = (uint32_t)sizeof decoder->input - keep;
    uint32_t want = space < left ? space : left;
    if (want > 0) {
        if (!decoder->source.read(decoder->source.context, at, decoder->input + keep, want)) {
            return false;
        }
        decoder->buffered = keep + want;
    }
    decoder->refill = false;
    return true;
}

static bool ensure_bytes(OpusDecoderContext *decoder, uint32_t needed, OpusResult *status)
{
    uint32_t available = decoder->buffered - decoder->consumed_in_buffer;
    if (available >= needed) {
        return true;
    }
    refill_buffer(decoder);
    available = decoder->buffered - decoder->consumed_in_buffer;
    if (available >= needed) {
        return true;
    }
    if (decoder->buffer_start + decoder->buffered >= decoder->source.size) {
        decoder->finished = true;
        *status = OPUS_FINISHED;
    } else {
        *status = OPUS_STARVED;
    }
    return false;
}

void opus_decoder_start(OpusDecoderContext *decoder, FlacInput source, uint8_t channels,
                        uint32_t sample_rate)
{
    (void)channels;
    (void)sample_rate;
    memset(decoder, 0, sizeof *decoder);
    decoder->source = source;
    decoder->channels = 2;
    decoder->st = (void *)decoder->state_pool;
    opus_decoder_init((OpusDecoder *)decoder->st, 48000, 2);
    decoder->refill = true;
}

OpusResult opus_decoder_step(OpusDecoderContext *decoder)
{
    if (decoder->finished) {
        return OPUS_FINISHED;
    }
    for (;;) {
        OpusResult res = OPUS_DECODED;

        if (decoder->demux_state == DEMUX_FIND_SYNC) {
            if (!ensure_bytes(decoder, 4, &res)) {
                return res;
            }
            const uint8_t *cur = decoder->input + decoder->consumed_in_buffer;
            if (cur[0] == 'O' && cur[1] == 'g' && cur[2] == 'g' && cur[3] == 'S') {
                decoder->sync_page_offset = decoder->buffer_start + decoder->consumed_in_buffer;
                decoder->demux_state = DEMUX_READ_HEADER;
            } else {
                decoder->consumed_in_buffer++;
            }
            continue;
        }

        if (decoder->demux_state == DEMUX_READ_HEADER) {
            if (!ensure_bytes(decoder, OGG_HEADER_BYTES, &res)) {
                return res;
            }
            const uint8_t *head = decoder->input + decoder->consumed_in_buffer;
            decoder->segments_count = head[26];
            decoder->consumed_in_buffer += OGG_HEADER_BYTES;
            decoder->demux_state = DEMUX_READ_SEGMENTS;
            continue;
        }

        if (decoder->demux_state == DEMUX_READ_SEGMENTS) {
            if (!ensure_bytes(decoder, decoder->segments_count, &res)) {
                return res;
            }
            memcpy(decoder->segment_table, decoder->input + decoder->consumed_in_buffer,
                   decoder->segments_count);
            decoder->consumed_in_buffer += decoder->segments_count;
            decoder->segment_index = 0;
            decoder->demux_state = DEMUX_READ_PACKET;
            continue;
        }

        if (decoder->demux_state == DEMUX_READ_PACKET) {
            if (decoder->segment_index >= decoder->segments_count) {
                decoder->demux_state = DEMUX_FIND_SYNC;
                continue;
            }
            uint32_t seg_len = decoder->segment_table[decoder->segment_index];
            if (!ensure_bytes(decoder, seg_len, &res)) {
                return res;
            }
            if (decoder->packet_len + seg_len <= sizeof decoder->packet) {
                memcpy(decoder->packet + decoder->packet_len,
                       decoder->input + decoder->consumed_in_buffer, seg_len);
                decoder->packet_len += seg_len;
            }
            decoder->consumed_in_buffer += seg_len;
            decoder->segment_index++;

            if (seg_len == 255) {
                continue;
            }

            uint32_t pkt_len = decoder->packet_len;
            decoder->packet_len = 0;

            if (!decoder->headers_read) {
                if (pkt_len >= 19 && memcmp(decoder->packet, "OpusHead", 8) == 0) {
                    uint8_t ch = decoder->packet[9];
                    if (ch == 1 || ch == 2) {
                        decoder->channels = ch;
                    }
                    decoder->pre_skip = (uint16_t)((uint16_t)decoder->packet[10] |
                                                   ((uint16_t)decoder->packet[11] << 8));
                    opus_decoder_init((OpusDecoder *)decoder->st, 48000, decoder->channels);
                } else if (pkt_len >= 8 && memcmp(decoder->packet, "OpusTags", 8) == 0) {
                    decoder->headers_read = true;
                }
                continue;
            }

            int16_t *pcm = (int16_t *)decoder->channel_samples;
            int ret = opus_decode((OpusDecoder *)decoder->st, decoder->packet, (opus_int32)pkt_len,
                                  pcm, OPUS_MAX_FRAME_SAMPLES, 0);
            if (ret <= 0) {
                continue;
            }
            uint32_t samples = (uint32_t)ret;
            uint32_t skip_now = 0;
            if (decoder->pre_skip > 0) {
                if (samples <= decoder->pre_skip) {
                    decoder->pre_skip -= samples;
                    continue;
                }
                skip_now = decoder->pre_skip;
                decoder->pre_skip = 0;
                samples -= skip_now;
            }
            if (decoder->channels == 1) {
                for (int i = (int)samples - 1; i >= 0; i--) {
                    uint32_t idx = (uint32_t)i;
                    int16_t s = pcm[skip_now + idx];
                    decoder->channel_samples[0][idx] = (int32_t)s;
                    decoder->channel_samples[1][idx] = (int32_t)s;
                }
            } else {
                for (int i = (int)samples - 1; i >= 0; i--) {
                    uint32_t idx = (uint32_t)i;
                    int16_t left = pcm[(skip_now + idx) * 2];
                    int16_t right = pcm[(skip_now + idx) * 2 + 1];
                    decoder->channel_samples[1][idx] = (int32_t)right;
                    decoder->channel_samples[0][idx] = (int32_t)left;
                }
            }
            decoder->decoded_samples = samples;
            return OPUS_DECODED;
        }
    }
}

uint32_t opus_decoder_position(const OpusDecoderContext *decoder)
{
    return decoder->sync_page_offset;
}

uint32_t opus_decoder_samples(const OpusDecoderContext *decoder)
{
    return decoder->decoded_samples;
}

uint8_t opus_decoder_channels(const OpusDecoderContext *decoder)
{
    return decoder->channels;
}

const int32_t *opus_decoder_channel(const OpusDecoderContext *decoder, int channel)
{
    int index = (channel > 0 && decoder->channels > 1) ? 1 : 0;
    return decoder->channel_samples[index];
}
