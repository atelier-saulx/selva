/* -------------------------------------------------------------------------
 * Works when compiled for either 32-bit or 64-bit targets, optimized for
 * 64 bit.
 *
 * Canonical implementation of Init/Update/Finalize for SHA-3 byte input.
 *
 * SHA3-256, SHA3-384, SHA-512 are implemented. SHA-224 can easily be added.
 *
 * Based on code from http://keccak.noekeon.org/ .
 *
 * I place the code that I wrote into public domain, free to use.
 *
 * I would appreciate if you give credits to this work if you used it to
 * write or test * your code.
 *
 * Aug 2015. Andrey Jivsov. crypto@brainhub.org
 *
 * Copyright (c) 2020 brainhub
 * Copyright (c) 2023 SAULX
 *
 * SPDX-License-Identifier: MIT
 * ---------------------------------------------------------------------- */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "sha3.h"

#define SHA3_ASSERT( x )
#define SHA3_TRACE( format, ...)
#define SHA3_TRACE_BUF(format, buf, l)

/*
 * This flag is used to configure "pure" Keccak, as opposed to NIST SHA3.
 */
#define SHA3_USE_KECCAK_FLAG 0x80000000
#define SHA3_CW(x) ((x) & (~SHA3_USE_KECCAK_FLAG))

#define KECCAK_ROUNDS 24

#if defined(_MSC_VER)
#define SHA3_CONST(x) x
#else
#define SHA3_CONST(x) x##L
#endif

#ifndef SHA3_ROTL64
#define SHA3_ROTL64(x, y) \
	(((x) << (y)) | ((x) >> ((sizeof(uint64_t)*8) - (y))))
#endif

static const uint64_t keccakf_rndc[24] = {
    SHA3_CONST(0x0000000000000001UL), SHA3_CONST(0x0000000000008082UL),
    SHA3_CONST(0x800000000000808aUL), SHA3_CONST(0x8000000080008000UL),
    SHA3_CONST(0x000000000000808bUL), SHA3_CONST(0x0000000080000001UL),
    SHA3_CONST(0x8000000080008081UL), SHA3_CONST(0x8000000000008009UL),
    SHA3_CONST(0x000000000000008aUL), SHA3_CONST(0x0000000000000088UL),
    SHA3_CONST(0x0000000080008009UL), SHA3_CONST(0x000000008000000aUL),
    SHA3_CONST(0x000000008000808bUL), SHA3_CONST(0x800000000000008bUL),
    SHA3_CONST(0x8000000000008089UL), SHA3_CONST(0x8000000000008003UL),
    SHA3_CONST(0x8000000000008002UL), SHA3_CONST(0x8000000000000080UL),
    SHA3_CONST(0x000000000000800aUL), SHA3_CONST(0x800000008000000aUL),
    SHA3_CONST(0x8000000080008081UL), SHA3_CONST(0x8000000000008080UL),
    SHA3_CONST(0x0000000080000001UL), SHA3_CONST(0x8000000080008008UL)
};

static const unsigned keccakf_rotc[24] = {
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62,
    18, 39, 61, 20, 44
};

static const unsigned keccakf_piln[24] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20,
    14, 22, 9, 6, 1
};

/* generally called after SHA3_KECCAK_SPONGE_WORDS-ctx->capacityWords words
 * are XORed into the state s
 */
static void
keccakf(uint64_t s[25], int keccak_rounds)
{
    for (int round = 0; round < keccak_rounds; round++) {
        uint64_t t, bc[5], bd[5];
        uint64_t tmp[25];

        /* Theta */
        for (int i = 0; i < 5; i++)
            bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];

        bd[0] = bc[4] ^ SHA3_ROTL64(bc[1], 1);
        bd[1] = bc[0] ^ SHA3_ROTL64(bc[2], 1);
        bd[2] = bc[1] ^ SHA3_ROTL64(bc[3], 1);
        bd[3] = bc[2] ^ SHA3_ROTL64(bc[4], 1);
        bd[4] = bc[3] ^ SHA3_ROTL64(bc[0], 1);
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 25; j += 5) {
                tmp[j + i] = s[j + i] ^ bd[i];
            }
        }

        /* Rho Pi */
        t = tmp[1];
        for (int i = 0; i < 24; i++) {
            int j = keccakf_piln[i];
            uint64_t prev = tmp[j];

            tmp[j] = SHA3_ROTL64(t, keccakf_rotc[i]);
            t = prev;
        }

        /* Chi */
        for (int j = 0; j < 25; j += 5) {
            uint64_t *p = &tmp[j];

            s[j + 0] = p[0] ^ ((~p[1]) & p[2]);
            s[j + 1] = p[1] ^ ((~p[2]) & p[3]);
            s[j + 2] = p[2] ^ ((~p[3]) & p[4]);
            s[j + 3] = p[3] ^ ((~p[4]) & p[0]);
            s[j + 4] = p[4] ^ ((~p[0]) & p[1]);
        }

        /* Iota */
        s[0] ^= keccakf_rndc[round];
    }
}

static void
sha3_Init(struct sha3_context *ctx, unsigned bitSize, int keccak_rounds) {
    assert((bitSize == 256 || bitSize == 384 || bitSize == 512) && keccak_rounds > 8);

    memset(ctx, 0, sizeof(*ctx));
    ctx->capacityWords = 2 * bitSize / (8 * sizeof(uint64_t));
    ctx->keccak_rounds = keccak_rounds;
}

void
sha3_Init256(struct sha3_context *ctx)
{
    sha3_Init(ctx, 256, KECCAK_ROUNDS);
}

void
sha3_Init256KitTen(struct sha3_context *ctx)
{
    sha3_Init(ctx, 256, 10);
}

void
sha3_Init384(struct sha3_context *ctx)
{
    sha3_Init(ctx, 384, KECCAK_ROUNDS);
}

void
sha3_Init512(struct sha3_context *ctx)
{
    sha3_Init(ctx, 512, KECCAK_ROUNDS);
}

enum sha3_flags
sha3_SetFlags(struct sha3_context *ctx, enum sha3_flags flags)
{
    flags &= SHA3_FLAGS_KECCAK;
    ctx->capacityWords |= (flags == SHA3_FLAGS_KECCAK ? SHA3_USE_KECCAK_FLAG : 0);
    return flags;
}

void
sha3_Update(struct sha3_context *ctx, void const *bufIn, size_t len)
{
    /* 0...7 -- how much is needed to have a word */
    unsigned old_tail = (8 - ctx->byteIndex) & 7;

    size_t words;
    unsigned tail;
    size_t i;

    const uint8_t *buf = bufIn;

    SHA3_TRACE_BUF("called to update with:", buf, len);

    SHA3_ASSERT(ctx->byteIndex < 8);
    SHA3_ASSERT(ctx->wordIndex < sizeof(ctx->u.s) / sizeof(ctx->u.s[0]));

    if(len < old_tail) {        /* have no complete word or haven't started
                                 * the word yet */
        SHA3_TRACE("because %d<%d, store it and return", (unsigned)len,
                (unsigned)old_tail);
        /* endian-independent code follows: */
        while (len--)
            ctx->saved |= (uint64_t) (*(buf++)) << ((ctx->byteIndex++) * 8);
        SHA3_ASSERT(ctx->byteIndex < 8);
        return;
    }

    if(old_tail) {              /* will have one word to process */
        SHA3_TRACE("completing one word with %d bytes", (unsigned)old_tail);
        /* endian-independent code follows: */
        len -= old_tail;
        while (old_tail--)
            ctx->saved |= (uint64_t) (*(buf++)) << ((ctx->byteIndex++) * 8);

        /* now ready to add saved to the sponge */
        ctx->u.s[ctx->wordIndex] ^= ctx->saved;
        SHA3_ASSERT(ctx->byteIndex == 8);
        ctx->byteIndex = 0;
        ctx->saved = 0;
        if(++ctx->wordIndex ==
                (SHA3_KECCAK_SPONGE_WORDS - SHA3_CW(ctx->capacityWords))) {
            keccakf(ctx->u.s, ctx->keccak_rounds);
            ctx->wordIndex = 0;
        }
    }

    /* now work in full words directly from input */

    SHA3_ASSERT(ctx->byteIndex == 0);

    words = len / sizeof(uint64_t);
    tail = len - words * sizeof(uint64_t);

    SHA3_TRACE("have %d full words to process", (unsigned)words);

    for(i = 0; i < words; i++, buf += sizeof(uint64_t)) {
        const uint64_t t = (uint64_t) (buf[0]) |
                ((uint64_t) (buf[1]) << 8 * 1) |
                ((uint64_t) (buf[2]) << 8 * 2) |
                ((uint64_t) (buf[3]) << 8 * 3) |
                ((uint64_t) (buf[4]) << 8 * 4) |
                ((uint64_t) (buf[5]) << 8 * 5) |
                ((uint64_t) (buf[6]) << 8 * 6) |
                ((uint64_t) (buf[7]) << 8 * 7);
#if defined(__x86_64__ ) || defined(__i386__)
        SHA3_ASSERT(memcmp(&t, buf, 8) == 0);
#endif
        ctx->u.s[ctx->wordIndex] ^= t;
        if(++ctx->wordIndex ==
                (SHA3_KECCAK_SPONGE_WORDS - SHA3_CW(ctx->capacityWords))) {
            keccakf(ctx->u.s, ctx->keccak_rounds);
            ctx->wordIndex = 0;
        }
    }

    SHA3_TRACE("have %d bytes left to process, save them", (unsigned)tail);

    /* finally, save the partial word */
    SHA3_ASSERT(ctx->byteIndex == 0 && tail < 8);
    while (tail--) {
        SHA3_TRACE("Store byte %02x '%c'", *buf, *buf);
        ctx->saved |= (uint64_t) (*(buf++)) << ((ctx->byteIndex++) * 8);
    }
    SHA3_ASSERT(ctx->byteIndex < 8);
    SHA3_TRACE("Have saved=0x%016" PRIx64 " at the end", ctx->saved);
}

/* This is simply the 'update' with the padding block.
 * The padding block is 0x01 || 0x00* || 0x80. First 0x01 and last 0x80
 * bytes are always present, but they can be the same byte.
 */
void const *
sha3_Finalize(struct sha3_context *ctx)
{
    SHA3_TRACE("called with %d bytes in the buffer", ctx->byteIndex);

    /* Append 2-bit suffix 01, per SHA-3 spec. Instead of 1 for padding we
     * use 1<<2 below. The 0x02 below corresponds to the suffix 01.
     * Overall, we feed 0, then 1, and finally 1 to start padding. Without
     * M || 01, we would simply use 1 to start padding. */

    uint64_t t;

    if( ctx->capacityWords & SHA3_USE_KECCAK_FLAG ) {
        /* Keccak version */
        t = (uint64_t)(((uint64_t) 1) << (ctx->byteIndex * 8));
    }
    else {
        /* SHA3 version */
        t = (uint64_t)(((uint64_t)(0x02 | (1 << 2))) << ((ctx->byteIndex) * 8));
    }

    ctx->u.s[ctx->wordIndex] ^= ctx->saved ^ t;

    ctx->u.s[SHA3_KECCAK_SPONGE_WORDS - SHA3_CW(ctx->capacityWords) - 1] ^=
            SHA3_CONST(0x8000000000000000UL);
    keccakf(ctx->u.s, ctx->keccak_rounds);

    /* Return first bytes of the ctx->s. This conversion is not needed for
     * little-endian platforms e.g. wrap with #if !defined(__BYTE_ORDER__)
     * || !defined(__ORDER_LITTLE_ENDIAN__) || __BYTE_ORDER__!=__ORDER_LITTLE_ENDIAN__
     *    ... the conversion below ...
     * #endif */
    for (unsigned i = 0; i < SHA3_KECCAK_SPONGE_WORDS; i++) {
        const unsigned t1 = (uint32_t) ctx->u.s[i];
        const unsigned t2 = (uint32_t) ((ctx->u.s[i] >> 16) >> 16);
        ctx->u.sb[i * 8 + 0] = (uint8_t) (t1);
        ctx->u.sb[i * 8 + 1] = (uint8_t) (t1 >> 8);
        ctx->u.sb[i * 8 + 2] = (uint8_t) (t1 >> 16);
        ctx->u.sb[i * 8 + 3] = (uint8_t) (t1 >> 24);
        ctx->u.sb[i * 8 + 4] = (uint8_t) (t2);
        ctx->u.sb[i * 8 + 5] = (uint8_t) (t2 >> 8);
        ctx->u.sb[i * 8 + 6] = (uint8_t) (t2 >> 16);
        ctx->u.sb[i * 8 + 7] = (uint8_t) (t2 >> 24);
    }

    SHA3_TRACE_BUF("Hash: (first 32 bytes)", ctx->u.sb, 256 / 8);

    return (ctx->u.sb);
}

sha3_return_t sha3_HashBuffer( unsigned bitSize, enum sha3_flags flags, const void *in, unsigned inBytes, void *out, unsigned outBytes ) {
    struct sha3_context c;

    sha3_Init(&c, bitSize, KECCAK_ROUNDS);

    if( sha3_SetFlags(&c, flags) != flags ) {
        return SHA3_RETURN_BAD_PARAMS;
    }

    sha3_Update(&c, in, inBytes);
    const void *h = sha3_Finalize(&c);

    if(outBytes > bitSize/8)
        outBytes = bitSize/8;
    memcpy(out, h, outBytes);
    return SHA3_RETURN_OK;
}
