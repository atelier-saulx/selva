#pragma once
#ifndef SELVA_LANG
#define SELVA_LANG

#if __APPLE__ && __MACH__
#include <xlocale.h>
#endif
#include <locale.h>

#define LANG_NAME_MAX 4ul
#define LANG_TERRITORY_MAX 4ul
#define LANG_MAX (LANG_NAME_MAX + LANG_TERRITORY_MAX)

locale_t SelvaLang_GetLocale(const char *lang_str, size_t lang_len);

#endif /* SELVA_LANG */
