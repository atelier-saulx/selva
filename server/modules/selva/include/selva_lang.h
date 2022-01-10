#pragma once
#ifndef SELVA_LANG_H
#define SELVA_LANG_H

#if __APPLE__ && __MACH__
#include <xlocale.h>
#endif
#include <locale.h>

#define LANG_NAME_MAX 4ul
#define LANG_TERRITORY_MAX 4ul
#define LANG_MAX (LANG_NAME_MAX + LANG_TERRITORY_MAX)

/**
 * Get locale for a lang string.
 */
locale_t SelvaLang_GetLocale(const char *lang_str, size_t lang_len);

#endif /* SELVA_LANG_H */
