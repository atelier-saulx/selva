#pragma once
#ifndef BASE64_H
#define BASE64_H

size_t base64_encode_s(unsigned char *out, const unsigned char *src, size_t len, size_t line_max);

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
unsigned char * base64_encode(const unsigned char *src, size_t len, size_t *out_len);

/**
 * Base64 decode.
 * Caller is responsible for freeing the returned buffer.
 * @param src Data to be decoded
 * @param len Length of the data to be decoded
 * @param out_len Pointer to output length variable
 * @returns Allocated buffer of out_len bytes of decoded data, or %NULL on failure
 */
unsigned char * base64_decode(const unsigned char *src, size_t len, size_t *out_len);

/**
 * Calculate the required buffer size of a string of n characters.
 * @param line_max is the max line length. 0 = no limit; 72 = typical.
 */
static size_t base64_out_len(size_t n, size_t line_max) {
    /* RFE Techincally this should be ok but with shorted padding */
#if 0
    return ((4 * n / 3) + 3) & ~3;
#endif
    size_t olen;

    olen = n * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
    olen += olen / line_max; /* line feeds */

    return olen;
}

#endif /* BASE64_H */
