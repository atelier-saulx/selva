/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Selva string flags.
 */
enum selva_string_flags {
    SELVA_STRING_CRC = 0x01, /*!< CRC enabled. */
    SELVA_STRING_FREEZE = 0x02, /*!< Permanently shared string; Shouldn't be freed. */
    SELVA_STRING_MUTABLE = 0x04, /*!< A mutable string. */
};

struct selva_string;

/**
 * Create a new string.
 */
struct selva_string *selva_string_create(const char *str, size_t len, enum selva_string_flags flags);

/**
 * Create a string using a printf format string.
 */
struct selva_string *selva_string_createf(const char *fmt, ...);

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
 * Get a pointer to the contained string.
 * @param s is a pointer to a selva_string.
 * @param[out] len is a pointer to a variable to store the length of s.
 */
const char *selva_string_get(struct selva_string *s, size_t *len);

/**
 * Get the currently set flags of the string s.
 * @param s is a pointer to a selva_string.
 */
enum selva_string_flags selva_string_get_flags(struct selva_string *s);

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
