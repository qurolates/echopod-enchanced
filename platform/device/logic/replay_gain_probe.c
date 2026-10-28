#include "device/replay_gain_probe.h"

#include <string.h>

static uint32_t r32be(const uint8_t *b)
{
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | (uint32_t)b[3];
}

static uint32_t r32le(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static uint32_t rsync(const uint8_t *b)
{
    return ((uint32_t)(b[0] & 0x7F) << 21) | ((uint32_t)(b[1] & 0x7F) << 14) |
           ((uint32_t)(b[2] & 0x7F) << 7) | (uint32_t)(b[3] & 0x7F);
}

static void probe_id3v2(FlacInput input, uint32_t base, ReplayGainValues *out)
{
    uint8_t h[10];
    if (base + 10 > input.size || !input.read(input.context, base, h, 10)) {
        return;
    }
    if (memcmp(h, "ID3", 3) != 0 || h[3] < 2 || h[3] > 4) {
        return;
    }
    uint8_t ver = h[3];
    uint32_t cur = base + 10;
    uint32_t end = cur + rsync(h + 6);
    if (end > input.size) {
        end = input.size;
    }
    while (cur + 10 <= end) {
        uint8_t fh[10];
        uint32_t hlen = ver == 2 ? 6u : 10u;
        if (!input.read(input.context, cur, fh, hlen) || fh[0] == 0) {
            break;
        }
        uint32_t fsz = ver == 2 ? ((uint32_t)fh[3] << 16 | (uint32_t)fh[4] << 8 | fh[5])
                                : (ver == 4 ? rsync(fh + 4) : r32be(fh + 4));
        cur += hlen;
        if (fsz > end - cur) {
            break;
        }
        bool is_txxx = (ver == 2 && memcmp(fh, "TXX", 3) == 0) ||
                       (ver > 2 && memcmp(fh, "TXXX", 4) == 0);
        if (is_txxx && fsz >= 2) {
            char b[96];
            uint32_t r = fsz < sizeof b ? fsz : (uint32_t)sizeof b;
            if (input.read(input.context, cur, b, r) && (b[0] == 0 || b[0] == 3)) {
                uint32_t klen = 0;
                while (1 + klen < r && b[1 + klen] != '\0') {
                    klen++;
                }
                if (1 + klen < r) {
                    replay_gain_assign_field(out, b + 1, klen, b + 2 + klen, r - (2 + klen));
                }
            }
        }
        cur += fsz;
    }
}

static void probe_vorbis(FlacInput input, uint32_t cur, uint32_t len, ReplayGainValues *out)
{
    uint8_t w[4];
    if (len < 8 || cur + 8 > input.size || !input.read(input.context, cur, w, 4)) {
        return;
    }
    uint32_t vlen = r32le(w);
    cur += 4 + vlen;
    if (cur + 4 > input.size || !input.read(input.context, cur, w, 4)) {
        return;
    }
    uint32_t count = r32le(w);
    cur += 4;
    for (uint32_t i = 0; i < count && i < 64 && cur + 4 <= input.size; i++) {
        if (!input.read(input.context, cur, w, 4)) {
            break;
        }
        uint32_t elen = r32le(w);
        cur += 4;
        if (elen > 128) {
            cur += elen;
            continue;
        }
        char buf[80];
        uint32_t r = elen < sizeof buf ? elen : (uint32_t)sizeof buf;
        if (input.read(input.context, cur, buf, r)) {
            for (uint32_t k = 0; k < r; k++) {
                if (buf[k] == '=') {
                    replay_gain_assign_field(out, buf, k, buf + k + 1, r - (k + 1));
                    break;
                }
            }
        }
        cur += elen;
    }
}

static void probe_flac(FlacInput input, ReplayGainValues *out)
{
    uint8_t h[10];
    uint32_t cur = 0;
    if (input.size < 14 || !input.read(input.context, 0, h, 10)) {
        return;
    }
    if (memcmp(h, "ID3", 3) == 0) {
        cur = 10 + rsync(h + 6);
    }
    if (cur + 4 > input.size || !input.read(input.context, cur, h, 4) || memcmp(h, "fLaC", 4) != 0) {
        return;
    }
    cur += 4;
    bool last = false;
    while (!last && cur + 4 <= input.size && input.read(input.context, cur, h, 4)) {
        last = (h[0] & 0x80) != 0;
        uint32_t type = h[0] & 0x7F;
        uint32_t blen = (uint32_t)h[1] << 16 | (uint32_t)h[2] << 8 | (uint32_t)h[3];
        cur += 4;
        if (type == 4) {
            probe_vorbis(input, cur, blen, out);
            break;
        }
        cur += blen;
    }
}

static void probe_opus(FlacInput input, ReplayGainValues *out)
{
    uint8_t p[27];
    if (input.size < 64 || !input.read(input.context, 0, p, 27) || memcmp(p, "OggS", 4) != 0) {
        return;
    }
    uint32_t cur = 27;
    for (uint32_t i = 0; i < p[26]; i++) {
        uint8_t s;
        if (!input.read(input.context, cur++, &s, 1)) {
            return;
        }
        cur += s;
    }
    if (cur + 27 > input.size || !input.read(input.context, cur, p, 27) || memcmp(p, "OggS", 4) != 0) {
        return;
    }
    uint32_t body_off = cur + 27 + p[26];
    uint8_t th[8];
    if (input.read(input.context, body_off, th, 8) && memcmp(th, "OpusTags", 8) == 0) {
        probe_vorbis(input, body_off + 8, input.size - (body_off + 8), out);
    }
}

static void probe_mp4_custom(FlacInput input, uint32_t cur, uint32_t end, ReplayGainValues *out)
{
    char name[48] = {0};
    uint32_t nlen = 0;
    char val[48] = {0};
    uint32_t vlen = 0;
    while (cur + 8 <= end) {
        uint8_t h[8];
        if (!input.read(input.context, cur, h, 8)) {
            break;
        }
        uint32_t sz = r32be(h);
        if (sz < 8 || sz > end - cur) {
            break;
        }
        if (memcmp(h + 4, "name", 4) == 0 && sz > 12) {
            uint32_t r = sz - 12 < sizeof name - 1 ? sz - 12 : (uint32_t)sizeof name - 1;
            if (input.read(input.context, cur + 12, name, r)) {
                nlen = r;
            }
        } else if (memcmp(h + 4, "data", 4) == 0 && sz > 16) {
            uint32_t r = sz - 16 < sizeof val - 1 ? sz - 16 : (uint32_t)sizeof val - 1;
            if (input.read(input.context, cur + 16, val, r)) {
                vlen = r;
            }
        }
        cur += sz;
    }
    if (nlen > 0 && vlen > 0) {
        replay_gain_assign_field(out, name, nlen, val, vlen);
    }
}

static uint32_t find_atom(FlacInput input, uint32_t cur, uint32_t end, const char *fourcc, uint32_t *sz_out)
{
    while (cur + 8 <= end) {
        uint8_t h[8];
        if (!input.read(input.context, cur, h, 8)) {
            break;
        }
        uint32_t sz = r32be(h);
        if (sz < 8 || sz > end - cur) {
            break;
        }
        if (memcmp(h + 4, fourcc, 4) == 0) {
            *sz_out = sz;
            return cur;
        }
        cur += sz;
    }
    return 0;
}

static void probe_mp4(FlacInput input, ReplayGainValues *out)
{
    uint32_t sz = 0;
    uint32_t lim = input.size < 65536 ? input.size : 65536;
    uint32_t moov = find_atom(input, 0, lim, "moov", &sz);
    if (moov == 0) {
        return;
    }
    uint32_t udta = find_atom(input, moov + 8, moov + sz, "udta", &sz);
    if (udta == 0) {
        return;
    }
    uint32_t meta = find_atom(input, udta + 8, udta + sz, "meta", &sz);
    if (meta == 0 || sz < 12) {
        return;
    }
    uint32_t ilst = find_atom(input, meta + 12, meta + sz, "ilst", &sz);
    if (ilst == 0) {
        return;
    }
    uint32_t cur = ilst + 8;
    uint32_t end = ilst + sz;
    while (cur + 8 <= end) {
        uint8_t h[8];
        if (!input.read(input.context, cur, h, 8)) {
            break;
        }
        uint32_t isz = r32be(h);
        if (isz < 8 || isz > end - cur) {
            break;
        }
        if (memcmp(h + 4, "----", 4) == 0) {
            probe_mp4_custom(input, cur + 8, cur + isz, out);
        }
        cur += isz;
    }
}

static void probe_wav(FlacInput input, ReplayGainValues *out)
{
    uint8_t h[12];
    if (input.size < 12 || !input.read(input.context, 0, h, 12)) {
        return;
    }
    if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) {
        return;
    }
    uint32_t cur = 12;
    for (int i = 0; i < 32 && cur + 8 <= input.size; i++) {
        uint8_t ch[8];
        if (!input.read(input.context, cur, ch, 8)) {
            break;
        }
        uint32_t sz = r32le(ch + 4);
        if (memcmp(ch, "id3 ", 4) == 0 || memcmp(ch, "ID3 ", 4) == 0) {
            probe_id3v2(input, cur + 8, out);
            break;
        }
        cur += 8 + sz + (sz & 1u);
    }
}

void replay_gain_probe_source(FlacInput input, AudioCodec codec, ReplayGainValues *out)
{
    if (out == NULL) {
        return;
    }
    *out = (ReplayGainValues){0};
    if (codec == AUDIO_CODEC_MP3) {
        probe_id3v2(input, 0, out);
    } else if (codec == AUDIO_CODEC_FLAC) {
        probe_flac(input, out);
    } else if (codec == AUDIO_CODEC_OPUS) {
        probe_opus(input, out);
    } else if (codec == AUDIO_CODEC_AAC) {
        probe_mp4(input, out);
    } else if (codec == AUDIO_CODEC_WAV) {
        probe_wav(input, out);
    }
}
