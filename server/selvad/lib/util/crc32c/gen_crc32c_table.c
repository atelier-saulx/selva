/*
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Copyright (C) 2013, 2021 Mark Adler <madler@alumni.caltech.edu>
 * SPDX-License-Identifier: Zlib
 */

/* Generate crc32c_table.h for crc32c.c. */

#include <stdio.h>
#include <stdint.h>

#define LONG 8192
#define SHORT 256

// Print a 2-D table of four-byte constants in hex.
static void print_table(uint32_t *tab, size_t rows, size_t cols, char *name) {
    printf("static uint32_t const %s[][%zu] = {\n", name, cols);
    size_t end = rows * cols;
    size_t k = 0;
    for (;;) {
        fputs("   {", stdout);
        size_t n = 0, j = 0;
        for (;;) {
            printf("0x%08x", tab[k + n]);
            if (++n == cols)
                break;
            putchar(',');
            if (++j == 6) {
                fputs("\n   ", stdout);
                j = 0;
            }
            putchar(' ');
        }
        k += cols;
        if (k == end)
            break;
        puts("},");
    }
    puts("}\n};");
}

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

static void crc32c_word_table(void) {
    uint32_t table[8][256];

    // Generate byte-wise table.
    for (unsigned n = 0; n < 256; n++) {
        uint32_t crc = ~n;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        table[0][n] = ~crc;
    }

    // Use byte-wise table to generate word-wise table.
    for (unsigned n = 0; n < 256; n++) {
        uint32_t crc = ~table[0][n];
        for (unsigned k = 1; k < 8; k++) {
            crc = table[0][crc & 0xff] ^ (crc >> 8);
            table[k][n] = ~crc;
        }
    }

    // Print table.
    print_table(table[0], 8, 256, "crc32c_table");
}

// Return a(x) multiplied by b(x) modulo p(x), where p(x) is the CRC
// polynomial. For speed, this requires that a not be zero.
static uint32_t multmodp(uint32_t a, uint32_t b) {
    uint32_t prod = 0;
    for (;;) {
        if (a & 0x80000000) {
            prod ^= b;
            if ((a & 0x7fffffff) == 0)
                break;
        }
        a <<= 1;
        b = b & 1 ? (b >> 1) ^ POLY : b >> 1;
    }
    return prod;
}

/* Take a length and build four lookup tables for applying the zeros operator
   for that length, byte-by-byte, on the operand. */
static void crc32c_zero_table(size_t len, char *name) {
    // Generate operator for len zeros.
    uint32_t op = 0x80000000;               // 1 (x^0)
    uint32_t sq = op >> 4;                  // x^4
    while (len) {
        sq = multmodp(sq, sq);              // x^2^(k+3), k == len bit position
        if (len & 1)
            op = multmodp(sq, op);
        len >>= 1;
    }

    // Generate table to update each byte of a CRC using op.
    uint32_t table[4][256];
    for (unsigned n = 0; n < 256; n++) {
        table[0][n] = multmodp(op, n);
        table[1][n] = multmodp(op, n << 8);
        table[2][n] = multmodp(op, n << 16);
        table[3][n] = multmodp(op, n << 24);
    }

    // Print the table to stdout.
    print_table(table[0], 4, 256, name);
}

int main(void) {
    puts(
"// crc32c.h\n"
"// Tables and constants for crc32c.c software and hardware calculations.\n"
"\n"
"// Table for a 64-bits-at-a-time software CRC-32C calculation. This table\n"
"// has built into it the pre and post bit inversion of the CRC.\n"
"// Copyright (C) 2013, 2021 Mark Adler <madler@alumni.caltech.edu>\n"
"// SPDX-License-Identifier: Zlib\n"
    );
    crc32c_word_table();
    puts(
"\n// Block sizes for three-way parallel crc computation.  LONG and SHORT\n"
"// must both be powers of two.  The associated string constants must be set\n"
"// accordingly, for use in constructing the assembler instructions."
        );
    printf("#define LONG %d\n", LONG);
    printf("#define LONGx1 \"%d\"\n", LONG);
    printf("#define LONGx2 \"%d\"\n", 2 * LONG);
    printf("#define SHORT %d\n", SHORT);
    printf("#define SHORTx1 \"%d\"\n", SHORT);
    printf("#define SHORTx2 \"%d\"\n", 2 * SHORT);
    puts(
"\n// Table to shift a CRC-32C by LONG bytes."
    );
    crc32c_zero_table(8192, "crc32c_long");
    puts(
"\n// Table to shift a CRC-32C by SHORT bytes."
    );
    crc32c_zero_table(256, "crc32c_short");
    return 0;
}
