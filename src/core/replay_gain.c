#include "core/replay_gain.h"

#include <stddef.h>

enum {
    GAIN_MAX_CB = 6000,
    GAIN_MIN_CB = -6000,
    Q16_SCALE = 65536,
    CB_TO_LOG2_Q16 = 108853,
    CB_DIVISOR = 1000
};

static bool same_key(const char *k, uint32_t len, const char *s)
{
    uint32_t i = 0;
    for (; i < len && s[i]; i++) {
        char c = k[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + 32);
        }
        if (c != s[i]) {
            return false;
        }
    }
    return i == len && s[i] == '\0';
}

static bool parse_num(const char *text, uint32_t len, bool *neg, uint32_t *ipart, uint32_t *fq16,
                      bool allow_db)
{
    if (text == NULL || len == 0 || neg == NULL || ipart == NULL || fq16 == NULL) {
        return false;
    }
    uint32_t pos = 0;
    while (pos < len && (uint8_t)text[pos] <= ' ') {
        pos++;
    }
    if (pos >= len) {
        return false;
    }
    *neg = text[pos] == '-';
    if (*neg || text[pos] == '+') {
        pos++;
    }
    uint32_t integer = 0;
    bool has_digits = false;
    while (pos < len && text[pos] >= '0' && text[pos] <= '9') {
        has_digits = true;
        integer = integer * 10u + (uint32_t)(text[pos++] - '0');
        if (integer > 100) {
            return false;
        }
    }
    uint32_t num = 0;
    uint32_t div = 1;
    if (pos < len && (text[pos] == '.' || text[pos] == ',')) {
        pos++;
        int count = 0;
        while (pos < len && text[pos] >= '0' && text[pos] <= '9') {
            has_digits = true;
            if (count < 4) {
                num = num * 10u + (uint32_t)(text[pos] - '0');
                div *= 10u;
                count++;
            }
            pos++;
        }
    }
    if (!has_digits) {
        return false;
    }
    while (pos < len && (uint8_t)text[pos] <= ' ') {
        pos++;
    }
    if (allow_db && pos + 1 < len && (text[pos] | 0x20) == 'd' && (text[pos + 1] | 0x20) == 'b') {
        pos += 2;
    }
    while (pos < len && (uint8_t)text[pos] <= ' ') {
        pos++;
    }
    if (pos < len && text[pos] != '\0') {
        return false;
    }
    *ipart = integer;
    *fq16 = (num * (uint32_t)Q16_SCALE + div / 2u) / div;
    return true;
}

bool replay_gain_parse_gain(const char *text, uint32_t length, int16_t *gain_cb)
{
    bool neg = false;
    uint32_t ipart = 0;
    uint32_t fq16 = 0;
    if (gain_cb == NULL || !parse_num(text, length, &neg, &ipart, &fq16, true) || ipart > 60) {
        return false;
    }
    int32_t total = (int32_t)(ipart * 100u + ((fq16 * 100u + 32768u) >> 16));
    if (neg) {
        total = -total;
    }
    if (total > GAIN_MAX_CB) {
        total = GAIN_MAX_CB;
    } else if (total < GAIN_MIN_CB) {
        total = GAIN_MIN_CB;
    }
    *gain_cb = (int16_t)total;
    return true;
}

bool replay_gain_parse_peak(const char *text, uint32_t length, uint32_t *peak_q16)
{
    bool neg = false;
    uint32_t ipart = 0;
    uint32_t fq16 = 0;
    if (peak_q16 == NULL || !parse_num(text, length, &neg, &ipart, &fq16, false) || neg) {
        return false;
    }
    uint32_t res = (ipart << 16) + fq16;
    if (res == 0) {
        return false;
    }
    *peak_q16 = res;
    return true;
}

bool replay_gain_assign_field(ReplayGainValues *values, const char *key, uint32_t key_len,
                             const char *value, uint32_t val_len)
{
    if (values == NULL || key == NULL || value == NULL || key_len < 11) {
        return false;
    }
    if (!same_key(key, 11, "replaygain_") && !same_key(key, 11, "replaygain-")) {
        return false;
    }
    const char *sub = key + 11;
    uint32_t sub_len = key_len - 11;
    if (same_key(sub, sub_len, "track_gain")) {
        return replay_gain_parse_gain(value, val_len, &values->track_gain_cb) &&
               (values->has_track_gain = true);
    }
    if (same_key(sub, sub_len, "album_gain")) {
        return replay_gain_parse_gain(value, val_len, &values->album_gain_cb) &&
               (values->has_album_gain = true);
    }
    if (same_key(sub, sub_len, "track_peak")) {
        return replay_gain_parse_peak(value, val_len, &values->track_peak_q16) &&
               (values->has_track_peak = true);
    }
    if (same_key(sub, sub_len, "album_peak")) {
        return replay_gain_parse_peak(value, val_len, &values->album_peak_q16) &&
               (values->has_album_peak = true);
    }
    return false;
}

static uint32_t pow2_q16(int32_t x_q16)
{
    int32_t k = x_q16 >> 16;
    uint32_t f = (uint32_t)x_q16 & 0xFFFFu;
    uint32_t y = (880u * f) >> 16;
    y = ((y + 3409u) * f) >> 16;
    y = ((y + 15831u) * f) >> 16;
    y = ((y + 45417u) * f) >> 16;
    uint32_t frac = (uint32_t)Q16_SCALE + y;
    if (k >= 0) {
        return k >= 15 ? 0x7FFFFFFFu : (frac << k);
    }
    int shift = -k;
    return shift >= 31 ? 0 : (frac >> shift);
}

uint32_t replay_gain_calculate_multiplier(const ReplayGainValues *values, ReplayGainMode mode)
{
    if (values == NULL || mode == REPLAY_GAIN_OFF) {
        return Q16_SCALE;
    }
    int16_t gain_cb = 0;
    bool has_peak = false;
    uint32_t peak_q16 = 0;
    if (mode == REPLAY_GAIN_TRACK && values->has_track_gain) {
        gain_cb = values->track_gain_cb;
        has_peak = values->has_track_peak;
        peak_q16 = values->track_peak_q16;
    } else if (mode == REPLAY_GAIN_ALBUM && values->has_album_gain) {
        gain_cb = values->album_gain_cb;
        has_peak = values->has_album_peak;
        peak_q16 = values->album_peak_q16;
    } else {
        return Q16_SCALE;
    }
    int32_t x_q16 = ((int32_t)gain_cb * CB_TO_LOG2_Q16) / CB_DIVISOR;
    uint32_t mult = pow2_q16(x_q16);
    if (has_peak && peak_q16 > 0) {
        uint64_t product = (uint64_t)mult * peak_q16;
        if (product > ((uint64_t)Q16_SCALE << 16)) {
            mult = (0xFFFFFFFFu / peak_q16) + ((0xFFFFFFFFu % peak_q16 + 1u) / peak_q16);
        }
    }
    return mult;
}
