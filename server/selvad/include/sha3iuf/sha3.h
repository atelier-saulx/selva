/*
 * Copyright (c) 2020 brainhub
 * Copyright (c) 2023 SAULX
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SHA3_H
#define SHA3_H

#include <stdint.h>

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
 * ---------------------------------------------------------------------- */

/* 'Words' here refers to uint64_t */
#define SHA3_KECCAK_SPONGE_WORDS \
	(((1600)/8/*bits to byte*/)/sizeof(uint64_t))
struct sha3_context {
    uint64_t saved;             /* the portion of the input message that we
                                 * didn't consume yet */
    union {                     /* Keccak's state */
        uint64_t s[SHA3_KECCAK_SPONGE_WORDS];
        uint8_t sb[SHA3_KECCAK_SPONGE_WORDS * 8];
    } u;
    unsigned byteIndex;         /* 0..7--the next byte after the set one
                                 * (starts from 0; 0--none are buffered) */
    unsigned wordIndex;         /* 0..24--the next word to integrate input
                                 * (starts from 0) */
    unsigned capacityWords;     /* the double size of the hash output in
                                 * words (e.g. 16 for Keccak 512) */
    int keccak_rounds;
};

enum sha3_flags {
    SHA3_FLAGS_NONE=0,
    SHA3_FLAGS_KECCAK=1
};

enum sha3_return {
    SHA3_RETURN_OK=0,
    SHA3_RETURN_BAD_PARAMS=1
};
typedef enum sha3_return sha3_return_t;

void sha3_Init256(struct sha3_context *ctx);

/**
 * SHA-3 KitTen variant.
 * Not that this is totally incompatible with the standard SHA-3 but it's fine
 * as long as it's only used internally.
 *
 * Too Much Crypto, Jean-Philippe Aumasson
 * https://eprint.iacr.org/2019/1492.pdf
 */
void
sha3_Init256KitTen(struct sha3_context *ctx);
void sha3_Init384(struct sha3_context *ctx);
void sha3_Init512(struct sha3_context *ctx);

enum sha3_flags sha3_SetFlags(struct sha3_context *ctx, enum sha3_flags);

void sha3_Update(struct sha3_context *ctx, void const *bufIn, size_t len);

void const *sha3_Finalize(struct sha3_context *ctx);

/* Single-call hashing */
sha3_return_t sha3_HashBuffer(
    unsigned bitSize,   /* 256, 384, 512 */
    enum sha3_flags flags, /* SHA3_FLAGS_NONE or SHA3_FLAGS_KECCAK */
    const void *in, unsigned inBytes,
    void *out, unsigned outBytes );     /* up to bitSize/8; truncation OK */

#endif
