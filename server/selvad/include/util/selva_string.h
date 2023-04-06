/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct finalizer;

/**
 * Selva string flags.
 */
enum selva_string_flags {
    /**
     * CRC enabled.
     */
    SELVA_STRING_CRC = 0x01,
    /**
     * Permanently shared string; Shouldn't be freed.
     */
    SELVA_STRING_FREEZE = 0x02,
    /**
     * A mutable string.
     */
    SELVA_STRING_MUTABLE = 0x04,
    /**
     * Fixed size mutable string.
     * Mutable only with selva_string_replace() and selva_string_to_mstr().
     */
    SELVA_STRING_MUTABLE_FIXED = 0x08,
    /**
     * Intern the string.
     * Similar to SELVA_STRING_FREEZE but tracked and shared internally.
     * Implies SELVA_STRING_FREEZE.
     */
    SELVA_STRING_INTERN = 0x10,
    /**
     * Compressed string.
     */
    SELVA_STRING_COMPRESS = 0x02,
    _SELVA_STRING_LAST_FLAG = 0x40,
};

struct selva_string;

/**
 * Find already interned string.
 */
struct selva_string *selva_string_find_intern(const char *str, size_t len);

/**
 * Create a new string.
 * @param str can be NULL.
 */
struct selva_string *selva_string_create(const char *str, size_t len, enum selva_string_flags flags);

/**
 * Create a string using a printf format string.
 */
struct selva_string *selva_string_createf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef _STDIO_H
/**
 * Read a string from a file directly into a new selva_string.
 * If the resulting string is shorter than `size` an errno is set. Use the
 * ferror() and feof() functions to distinguish between a read error and an
 * end-of-file.
 * @param flags can be SELVA_STRING_CRC | SELVA_STRING_COMPRESS.
 */
struct selva_string *selva_string_fread(FILE *fp, size_t size, enum selva_string_flags flags);
#endif

/**
 * Create a compressed string.
 * Note that most of the selva_string functions don't know how to handle with
 * compressed strings and will just assume it's a regular string.
 * @param flags Compressed strings can't handle most of the flags but notably
 *              SELVA_STRING_CRC is supported.
 */
struct selva_string *selva_string_createz(const char *in_str, size_t in_len, enum selva_string_flags flags);

/**
 * Decompress a compressed string.
 * @param s is a pointer to a compressed selva_string.
 * @param buf is where the decompressed string will be copied to.
 * @returns 0 if succeeded;
 *          SELVA_PROTO_EINTYPE if not a compressed string;
 *          SELVA_EINVAL if the string cannot be decompressed.
 */
int selva_string_decompress(const struct selva_string *s, char *buf);

/**
 * Duplicate a string.
 * @param s is a pointer to a selva_string.
 */
struct selva_string *selva_string_dup(const struct selva_string *s, enum selva_string_flags flags);

/**
 * Truncate the string s to a new length of newlen.
 * s must be mutable.
 * @param s is a pointer to a selva_string.
 * @param newlen is the new length of the string.
 * @returns 0 if succeeded; Otherwise an error code.
 */
int selva_string_truncate(struct selva_string *s, size_t newlen);

/**
 * Append str of length len to the string s.
 * s must be mutable.
 * @param s is a pointer to a selva_string.
 * @returns 0 if succeeded; Otherwise an error code.
 */
int selva_string_append(struct selva_string *s, const char *str, size_t len);

/**
 * Replace current value of the string s with str.
 * s must be mutable.
 * @returns 0 if succeeded; Otherwise an error code.
 */
int selva_string_replace(struct selva_string *s, const char *str, size_t len);

/**
 * Allows selva_string_free() to be passed to finalizer_add() and other similar
 * functions accepting void functions.
 */
typedef union {
    struct selva_string *__s;
    void *__p;
} _selva_string_ptr_t __attribute__((__transparent_union__));


/**
 * Free the strings s.
 * @param s is a pointer to a selva_string.
 */
void selva_string_free(_selva_string_ptr_t s);

/**
 * Add a selva_string to the given finalizer.
 * @param finalizer is a pointer to a finalizer.
 * @param s is a pointer to a selva_string.
 */
void selva_string_auto_finalize(struct finalizer *finalizer, struct selva_string *s);

/**
 * Get the currently set flags of the string s.
 * @param s is a pointer to a selva_string.
 */
enum selva_string_flags selva_string_get_flags(const struct selva_string *s);

/**
 * Get uncompressed length.
 */
size_t selva_string_getz_ulen(const struct selva_string *s);

/**
 * Get compression ratio.
 */
int selva_string_getz_cratio(const struct selva_string *s);

/**
 * Get a pointer to the contained C-string.
 * @param s is a pointer to a selva_string.
 * @param[out] len is a pointer to a variable to store the length of s.
 * @retruns Returns a pointer to the C-string.
 */
const char *selva_string_to_str(const struct selva_string *s, size_t *len);

/**
 * Get a pointer to the mutable C-string.
 * @param s is a pointer to a selva_string.
 * @param[out] len is a pointer to a variable to store the length of s.
 * @returns Returns a pointer to the C-string if the string is mutable; Otherwise a NULL pointer is returned.
 */
char *selva_string_to_mstr(struct selva_string *s, size_t *len);

/**
 * Convert a string into a long long integer.
 */
int selva_string_to_ll(const struct selva_string *s, long long *ll);

/**
 * Convert a string into an unsigned long long integer.
 */
int selva_string_to_ull(const struct selva_string *s, unsigned long long *ull);

/**
 * Convert a string into a float.
 */
int selva_string_to_float(const struct selva_string *s, float *f);

/**
 * Convert a string into a double.
 */
int selva_string_to_double(const struct selva_string *s, double *d);

/**
 * Convert a string into a long double.
 */
int selva_string_to_ldouble(const struct selva_string *s, long double *ld);

/**
 * Freeze the string s in memory.
 * Freezing a string allows sharing it in memory between multiple users and
 * disallows freeing it until the program exits.
 * This function can be called even if the string is immutable.
 * @param s is a pointer to a selva_string.
 */
void selva_string_freeze(struct selva_string *s);

/**
 * Enable CRC checking for the strings s.
 * This function can be called even if the string is immutable.
 * @param s is a pointer to a selva_string.
 */
void selva_string_en_crc(struct selva_string *s);

/**
 * Verify the CRC of the string s.
 * @param s is a pointer to a selva_string.
 */
int selva_string_verify_crc(struct selva_string *s);

/**
 * Set SELVA_STRING_COMPRESS flag on an existing string.
 * Setting the flag wont compress the string but mark it as compressed;
 * i.e. a metadata update.
 */
void selva_string_set_compress(struct selva_string *s);

/**
 * Compare two strings.
 * @param a is a pointer to the first string to be compared.
 * @param b is a pointer to the second strings to be compared.
 * @returns < 0 if the first character that does not match has a lower value in ptr1 than in ptr2;
 *            0 if the contents of both strings are equal;
 *          > 0 if the first character that does not match has a greater value in ptr1 than in ptr2.
 */
int selva_string_cmp(const struct selva_string *a, const struct selva_string *b);

ssize_t selva_string_strstr(struct selva_string *s, const char *sub_str, size_t sub_len);
