#pragma once
#ifndef SELVA_LANG
#define SELVA_LANG

locale_t SelvaLang_GetLocale(const char *lang_str, size_t lang_len);

/**
 * Transforms a given string into a locale dependant collated
 * case-insensitive non-human-readable binary representation that works works
 * with strcmp().
 * If dst is NULL then a number of bytes needed for the destination is returned.
 */
size_t SelvaLang_Strcasexfrm(char *dst, const char *src, locale_t locale);

#endif /* SELVA_LANG */
