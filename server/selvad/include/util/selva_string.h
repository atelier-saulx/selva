/*
 * Copyright (c) 2022 SAULX
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
     * Intern the string.
     * Similar to SELVA_STRING_FREEZE but tracked and shared internally.
     * Implies SELVA_STRING_FREEZE.
     */
    SELVA_STRING_INTERN = 0x08,
    _SELVA_STRING_LAST_FLAG = 0x10,
};

struct selva_string;

/**
 * Create a new string.
 */
struct selva_string *selva_string_create(const char *str, size_t len, enum selva_string_flags flags);

/**
 * Create a string using a printf format string.
 */
struct selva_string *selva_string_createf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Duplicate a string.
 * @param s is a pointer to a selva_string.
 */
struct selva_string *selva_string_dup(struct selva_string *s, enum selva_string_flags flags);

/**
 * Truncate the string s to a new length of newlen.
 * @param s is a pointer to a selva_string.
 * @param newlen is the new length of the string.
 * @returns 0 if succeeded; Otherwise an error code.
 */
int selva_string_truncate(struct selva_string *s, size_t newlen);

/**
 * Append str of length len to the string s.
 * @param s is a pointer to a selva_string.
 * @returns 0 if succeeded; Otherwise an error code.
 */
int selva_string_append(struct selva_string *s, const char *str, size_t len);

/**
 * Free the strings s.
 * @param s is a pointer to a selva_string.
 */
void selva_string_free(struct selva_string *s);

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
 * Get a pointer to the contained string.
 * @param s is a pointer to a selva_string.
 * @param[out] len is a pointer to a variable to store the length of s.
 */
const char *selva_string_to_str(const struct selva_string *s, size_t *len);

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
 * Compare two strings.
 * @param a is a pointer to the first string to be compared.
 * @param b is a pointer to the second strings to be compared.
 * @returns < 0 if the first character that does not match has a lower value in ptr1 than in ptr2;
 *            0 if the contents of both strings are equal;
 *          > 0 if the first character that does not match has a greater value in ptr1 than in ptr2.
 */
int selva_string_cmp(const struct selva_string *a, const struct selva_string *b);
