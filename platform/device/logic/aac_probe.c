#include "device/aac_probe.h"

#include <stddef.h>

enum {
    ID3V2_HEADER_SIZE = 10,
    ADTS_HEADER_MIN = 7,
    DEFAULT_AAC_FRAME_LEN = 360,
    ATOM_HEADER_SIZE = 8
};

static const uint32_t AAC_RATES[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static uint64_t read_be64(const uint8_t *data)
{
    return ((uint64_t)read_be32(data) << 32) | (uint64_t)read_be32(data + 4);
}

static bool same_fourcc(const uint8_t *data, const char *tag)
{
    return data[0] == (uint8_t)tag[0] && data[1] == (uint8_t)tag[1] &&
           data[2] == (uint8_t)tag[2] && data[3] == (uint8_t)tag[3];
}

static bool is_adts_header(const uint8_t *bytes)
{
    uint16_t sync = (uint16_t)(((uint16_t)bytes[0] << 4) | (uint16_t)(bytes[1] >> 4));
    if (sync != 0xFFF) {
        return false;
    }
    uint8_t layer = (bytes[1] >> 1) & 0x03;
    uint8_t rate_idx = (bytes[2] >> 2) & 0x0F;
    return layer == 0 && rate_idx < sizeof AAC_RATES / sizeof AAC_RATES[0];
}

bool aac_signature(const uint8_t *bytes, uint32_t length)
{
    if (length >= 8 && same_fourcc(bytes + 4, "ftyp")) {
        return true;
    }
    if (length >= 2 && bytes[0] == 0xFF && (bytes[1] & 0xF6) == 0xF0) {
        return true;
    }
    if (length >= 10 && bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3') {
        return true;
    }
    return false;
}

static uint32_t skip_id3v2(FlacInput input)
{
    uint8_t header[ID3V2_HEADER_SIZE];
    if (input.size < ID3V2_HEADER_SIZE || !input.read(input.context, 0, header, sizeof header)) {
        return 0;
    }
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        return 0;
    }
    uint32_t tag_size = ((uint32_t)(header[6] & 0x7F) << 21) |
                        ((uint32_t)(header[7] & 0x7F) << 14) |
                        ((uint32_t)(header[8] & 0x7F) << 7) |
                        (uint32_t)(header[9] & 0x7F);
    uint32_t total = ID3V2_HEADER_SIZE + tag_size;
    return total < input.size ? total : input.size;
}

static bool probe_adts(FlacInput input, AacStream *stream)
{
    uint32_t offset = skip_id3v2(input);
    uint8_t header[ADTS_HEADER_MIN];
    uint32_t limit = input.size > offset + 4096 ? offset + 4096 : input.size;
    while (offset + ADTS_HEADER_MIN <= limit) {
        if (!input.read(input.context, offset, header, sizeof header)) {
            return false;
        }
        if (is_adts_header(header)) {
            break;
        }
        offset++;
    }
    if (offset + ADTS_HEADER_MIN > limit || !is_adts_header(header)) {
        return false;
    }
    uint8_t rate_idx = (header[2] >> 2) & 0x0F;
    uint8_t channel_cfg = (uint8_t)(((header[2] & 0x01) << 2) | (header[3] >> 6));
    uint32_t frame_len = (((uint32_t)header[3] & 0x03) << 11) |
                         ((uint32_t)header[4] << 3) |
                         (uint32_t)(header[5] >> 5);
    if (frame_len < ADTS_HEADER_MIN) {
        frame_len = DEFAULT_AAC_FRAME_LEN;
    }
    stream->sample_rate = AAC_RATES[rate_idx];
    stream->channels = channel_cfg > 0 ? channel_cfg : 2;
    stream->samples_per_frame = 1024;
    stream->audio_start = offset;
    stream->audio_end = input.size;
    stream->stco_offset = 0;
    stream->chunk_count = 0;
    stream->is_mp4 = false;
    stream->is_co64 = false;
    uint32_t audio_bytes = input.size - offset;
    uint64_t frame_count = (uint64_t)audio_bytes / frame_len;
    stream->total_samples = (uint32_t)(frame_count * 1024);
    return true;
}

typedef struct AtomHeader {
    uint32_t offset;
    uint32_t body;
    uint32_t end;
    uint8_t type[4];
} AtomHeader;

static bool read_atom(FlacInput input, uint32_t offset, uint32_t limit, AtomHeader *atom)
{
    if (offset >= limit || limit - offset < ATOM_HEADER_SIZE) {
        return false;
    }
    uint8_t buf[16];
    if (!input.read(input.context, offset, buf, ATOM_HEADER_SIZE)) {
        return false;
    }
    uint64_t size = read_be32(buf);
    uint32_t header_len = ATOM_HEADER_SIZE;
    if (size == 1) {
        if (limit - offset < 16 || !input.read(input.context, offset, buf, 16)) {
            return false;
        }
        size = read_be64(buf + 8);
        header_len = 16;
    } else if (size == 0) {
        size = limit - offset;
    }
    if (size < header_len || offset + size > limit) {
        return false;
    }
    atom->offset = offset;
    atom->body = offset + header_len;
    atom->end = (uint32_t)(offset + size);
    atom->type[0] = buf[4];
    atom->type[1] = buf[5];
    atom->type[2] = buf[6];
    atom->type[3] = buf[7];
    return true;
}

static bool find_atom(FlacInput input, uint32_t start, uint32_t end, const char *type,
                      AtomHeader *found)
{
    uint32_t cursor = start;
    AtomHeader atom;
    while (read_atom(input, cursor, end, &atom)) {
        if (same_fourcc(atom.type, type)) {
            *found = atom;
            return true;
        }
        cursor = atom.end;
    }
    return false;
}

static bool probe_mp4(FlacInput input, AacStream *stream)
{
    AtomHeader ftyp;
    find_atom(input, 0, input.size, "ftyp", &ftyp);
    AtomHeader mdat;
    if (!find_atom(input, 0, input.size, "mdat", &mdat)) {
        return false;
    }
    AtomHeader moov;
    if (!find_atom(input, 0, input.size, "moov", &moov)) {
        return false;
    }
    uint32_t sample_rate = 44100;
    uint8_t channels = 2;
    uint32_t total_samples = 0;
    uint32_t audio_start = mdat.body;
    uint32_t stco_offset = 0;
    uint32_t chunk_count = 0;
    bool is_co64 = false;
    bool found_audio = false;

    AtomHeader trak;
    uint32_t cursor = moov.body;
    while (read_atom(input, cursor, moov.end, &trak)) {
        if (same_fourcc(trak.type, "trak")) {
            AtomHeader mdia, minf, stbl, stsd, mp4a;
            if (find_atom(input, trak.body, trak.end, "mdia", &mdia) &&
                find_atom(input, mdia.body, mdia.end, "minf", &minf) &&
                find_atom(input, minf.body, minf.end, "stbl", &stbl) &&
                find_atom(input, stbl.body, stbl.end, "stsd", &stsd) &&
                find_atom(input, stsd.body + 8, stsd.end, "mp4a", &mp4a)) {
                found_audio = true;
                uint8_t audio_entry[28];
                if (mp4a.end - mp4a.body >= 28 &&
                    input.read(input.context, mp4a.body, audio_entry, sizeof audio_entry)) {
                    channels = audio_entry[17];
                    uint16_t rate16 =
                        (uint16_t)(((uint16_t)audio_entry[22] << 8) | audio_entry[23]);
                    if (rate16 > 0) {
                        sample_rate = rate16;
                    }
                }
                AtomHeader mdhd;
                if (find_atom(input, mdia.body, mdia.end, "mdhd", &mdhd)) {
                    uint8_t mdhd_buf[32];
                    uint32_t len = mdhd.end - mdhd.body < 32 ? mdhd.end - mdhd.body : 32;
                    if (input.read(input.context, mdhd.body, mdhd_buf, len)) {
                        uint32_t timescale = 0;
                        uint64_t duration = 0;
                        if (mdhd_buf[0] == 0 && len >= 24) {
                            timescale = read_be32(mdhd_buf + 12);
                            duration = read_be32(mdhd_buf + 16);
                        } else if (mdhd_buf[0] == 1 && len >= 32) {
                            timescale = read_be32(mdhd_buf + 20);
                            duration = read_be64(mdhd_buf + 24);
                        }
                        if (timescale > 0) {
                            sample_rate = timescale;
                            total_samples = (uint32_t)duration;
                        }
                    }
                }
                AtomHeader stsz;
                if (find_atom(input, stbl.body, stbl.end, "stsz", &stsz)) {
                    uint8_t stsz_buf[12];
                    if (stsz.end - stsz.body >= 12 &&
                        input.read(input.context, stsz.body, stsz_buf, sizeof stsz_buf)) {
                        uint32_t sample_count = read_be32(stsz_buf + 8);
                        if (total_samples == 0 && sample_count > 0) {
                            total_samples = sample_count * 1024;
                        }
                    }
                }
                AtomHeader stco;
                if (find_atom(input, stbl.body, stbl.end, "stco", &stco)) {
                    uint8_t stco_buf[12];
                    if (stco.end - stco.body >= 12 &&
                        input.read(input.context, stco.body, stco_buf, sizeof stco_buf)) {
                        chunk_count = read_be32(stco_buf + 4);
                        if (chunk_count > 0) {
                            audio_start = read_be32(stco_buf + 8);
                            stco_offset = stco.body + 8;
                            is_co64 = false;
                        }
                    }
                } else if (find_atom(input, stbl.body, stbl.end, "co64", &stco)) {
                    uint8_t co64_buf[16];
                    if (stco.end - stco.body >= 16 &&
                        input.read(input.context, stco.body, co64_buf, sizeof co64_buf)) {
                        chunk_count = read_be32(co64_buf + 4);
                        if (chunk_count > 0) {
                            audio_start = (uint32_t)read_be64(co64_buf + 8);
                            stco_offset = stco.body + 8;
                            is_co64 = true;
                        }
                    }
                }
                break;
            }
        }
        cursor = trak.end;
    }
    if (!found_audio) {
        return false;
    }
    stream->sample_rate = sample_rate;
    stream->channels = channels > 0 ? channels : 2;
    stream->samples_per_frame = 1024;
    stream->total_samples = total_samples;
    stream->audio_start = audio_start;
    stream->audio_end = mdat.end <= input.size ? mdat.end : input.size;
    stream->stco_offset = stco_offset;
    stream->chunk_count = chunk_count;
    stream->is_mp4 = true;
    stream->is_co64 = is_co64;
    return true;
}

bool aac_probe(FlacInput input, AacStream *stream)
{
    if (probe_adts(input, stream)) {
        return true;
    }
    return probe_mp4(input, stream);
}

uint32_t aac_seek_offset(FlacInput input, const AacStream *stream, uint32_t sample,
                         uint32_t *actual_sample)
{
    if (stream->total_samples == 0 || sample == 0) {
        if (actual_sample != NULL) {
            *actual_sample = 0;
        }
        return stream->audio_start;
    }
    uint32_t clamped = sample < stream->total_samples ? sample : stream->total_samples;
    if (stream->is_mp4 && stream->chunk_count > 0 && input.read != NULL) {
        uint32_t chunk_idx =
            (uint32_t)((uint64_t)clamped * stream->chunk_count / stream->total_samples);
        if (chunk_idx >= stream->chunk_count) {
            chunk_idx = stream->chunk_count - 1;
        }
        uint32_t offset = stream->audio_start;
        if (stream->is_co64) {
            uint8_t buf[8];
            if (input.read(input.context, stream->stco_offset + chunk_idx * 8, buf, sizeof buf)) {
                offset = (uint32_t)read_be64(buf);
            }
        } else {
            uint8_t buf[4];
            if (input.read(input.context, stream->stco_offset + chunk_idx * 4, buf, sizeof buf)) {
                offset = read_be32(buf);
            }
        }
        if (actual_sample != NULL) {
            *actual_sample =
                (uint32_t)((uint64_t)chunk_idx * stream->total_samples / stream->chunk_count);
        }
        return offset;
    }
    uint64_t span = stream->audio_end - stream->audio_start;
    uint32_t offset = stream->audio_start + (uint32_t)(span * clamped / stream->total_samples);
    if (actual_sample != NULL) {
        *actual_sample = clamped;
    }
    return offset < stream->audio_end ? offset : stream->audio_end;
}
