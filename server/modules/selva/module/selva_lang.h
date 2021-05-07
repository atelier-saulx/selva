#pragma once
#ifndef SELVA_LANG
#define SELVA_LANG

#if __APPLE__ && __MACH__
#include <xlocale.h>
#endif
#include <locale.h>

locale_t SelvaLang_GetLocale(const char *lang_str, size_t lang_len);

#endif /* SELVA_LANG */
