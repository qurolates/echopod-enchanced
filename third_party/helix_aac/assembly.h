#ifndef _ASSEMBLY_H
#define _ASSEMBLY_H

typedef long long Word64;

static inline int MULSHIFT32(int x, int y)
{
    return (int)(((Word64)x * (Word64)y) >> 32);
}

static inline short CLIPTOSHORT(int x)
{
    int sign = x >> 31;
    if (sign != (x >> 15)) {
        x = sign ^ ((1 << 15) - 1);
    }
    return (short)x;
}

static inline int FASTABS(int x)
{
    int sign = x >> (sizeof(int) * 8 - 1);
    x ^= sign;
    x -= sign;
    return x;
}

static inline int CLZ(int x)
{
    return x ? __builtin_clz(x) : 32;
}

typedef union _U64 {
    Word64 w64;
    struct {
        unsigned int lo32;
        signed int   hi32;
    } r;
} U64;

static inline Word64 MADD64(Word64 sum64, int x, int y)
{
    return sum64 + ((Word64)x * (Word64)y);
}

#define CLIP_2N(y, n) do { \
    int _sign = (y) >> 31; \
    if (_sign != (y) >> (n)) { \
        (y) = _sign ^ ((1 << (n)) - 1); \
    } \
} while (0)

#define CLIP_2N_SHIFT(y, n) do { \
    int _sign = (y) >> 31; \
    if (_sign != (y) >> (30 - (n))) { \
        (y) = _sign ^ (0x3fffffff); \
    } else { \
        (y) = (y) << (n); \
    } \
} while (0)

#endif
