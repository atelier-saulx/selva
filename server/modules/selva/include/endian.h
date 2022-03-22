#pragma once
#ifndef _SELVA_ENDIAN_H_
#define _SELVA_ENDIAN_H_

#include <stdint.h>

_Static_assert(sizeof(double) == 8, "Only 64bit doubles are supported");

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define htobe16(x) __builtin_bswap16(x)
#define htole16(x) (x)
#define be16toh(x) __builtin_bswap16(x)
#define le16toh(x) (x)

#define htobe32(x) __builtin_bswap32(x)
#define htole32(x) (x)
#define be32toh(x) __builtin_bswap32(x)
#define le32toh(x) (x)

#define htobe64(x) __builtin_bswap64(x)
#define htole64(x) (x)
#define be64toh(x) __builtin_bswap64(x)
#define le64toh(x) (x)

static inline void htoledouble(char buf[8], double x) {
#if __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
    /*
     * x: 4 5 6 7  0 1 2 3
     *    0 1 2 3  4 5 6 7
     */
    char s[8];

    __builtin_memcpy(s, &x, 8);
    buf[0] = s[4];
    buf[1] = s[5];
    buf[2] = s[6];
    buf[3] = s[7];
    buf[4] = s[0];
    buf[5] = s[1];
    buf[6] = s[2];
    buf[7] = s[3];
#else
    __builtin_memcpy(buf, &x, 8);
#endif
}

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define htobe16(x) (x)
#define htole16(x) __builtin_bswap16(x)
#define be16toh(x) (x)
#define le16toh(x) __builtin_bswap16(x)

#define htobe32(x) (x)
#define htole32(x) __builtin_bswap32(x)
#define be32toh(x) (x)
#define le32toh(x) __builtin_bswap32(x)

#define htobe64(x) (x)
#define htole64(x) __builtin_bswap64(x)
#define be64toh(x) (x)
#define le64toh(x) __builtin_bswap64(x)

static inline void htoledouble(char buf[8], double x) {
#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /*
     * x: 3 2 1 0  7 6 5 4
     * s: 0 1 2 3  4 5 6 7
     */
    char s[8];

    __builtin_memcpy(s, &x, 8);
    buf[0] = s[3];
    buf[1] = s[2];
    buf[2] = s[1];
    buf[3] = s[0];
    buf[4] = s[7];
    buf[5] = s[6];
    buf[6] = s[5];
    buf[7] = s[4];
#else
    /*
     * x: 7 6 5 4  3 2 1 0
     * s: 0 1 2 3  4 5 6 7
     */
    char s[8];

    __builtin_memcpy(s, &x, 8);
    buf[0] = s[7];
    buf[1] = s[6];
    buf[2] = s[5];
    buf[3] = s[4];
    buf[4] = s[3];
    buf[5] = s[2];
    buf[6] = s[1];
    buf[7] = s[0];
#endif
}

#else
#error "Machine byte order not supported"
#endif

#endif /* _SELVA_ENDIAN_H_ */
