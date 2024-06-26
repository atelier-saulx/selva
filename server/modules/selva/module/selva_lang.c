/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include "redismodule.h"
#include "selva.h"
#include "jemalloc.h"
#include "selva_onload.h"
#include "selva_lang.h"
#include "selva_object.h"

#define FALLBACK_LANG "en"

#define FORALL_LANGS(apply) \
    apply(af, af_ZA) \
    apply(am, am_ET) \
    apply(be, be_BY) \
    apply(bg, bg_BG) \
    apply(ca, ca_ES) \
    apply(cs, cs_CZ) \
    apply(da, da_DK) \
    apply(de, de_DE) \
    apply(el, el_GR) \
    apply(en, en_GB) \
    apply(es, es_ES) \
    apply(et, et_EE) \
    apply(eu, eu_ES) \
    apply(fi, fi_FI) \
    apply(fr, fr_FR) \
    apply(gsw, gsw_CH) \
    apply(he, he_IL) \
    apply(hr, hr_HR) \
    apply(hu, hu_HU) \
    apply(hy, hy_AM) \
    apply(is, is_IS) \
    apply(it, it_IT) \
    apply(ja, ja_JP) \
    apply(kk, kk_KZ) \
    apply(ko, ko_KR) \
    apply(lt, lt_LT) \
    apply(nb, nb_NO) \
    apply(nl, nl_NL) \
    apply(nn, nn_NO) \
    apply(pl, pl_PL) \
    apply(pt, pt_PT) \
    apply(ro, ro_RO) \
    apply(ru, ru_RU) \
    apply(sk, sk_SK) \
    apply(sl, sl_SI) \
    apply(sr, sr_RS) \
    apply(sv, sv_SE) \
    apply(tr, tr_TR) \
    apply(uk, uk_UA) \
    apply(zh, zh_CN)

struct SelvaLang {
    __nonstring char name[LANG_NAME_MAX];
    __nonstring char territory[LANG_NAME_MAX];
    locale_t locale;
};

static void SelvaLang_Reply(struct RedisModuleCtx *ctx, void *p);
static void SelvaLang_Free(void *p);

static const struct SelvaObjectPointerOpts obj_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_LANG,
    .ptr_reply = SelvaLang_Reply,
    .ptr_free = SelvaLang_Free,
};

static struct SelvaObject *langs;

static void get_territory(char dst[4], const char *locale_name) {
    const char *lang;
    const char *territory_str;
    size_t territory_len;

    lang = strchr(locale_name, '_');
    if (!lang) {
        return;
    }

    territory_str = lang + 1;
    const char *s = strchr(territory_str, '.');
    territory_len = s ? (size_t)(s - territory_str) : strnlen(territory_str, LANG_TERRITORY_MAX);

    memcpy(dst, territory_str, min(territory_len, LANG_NAME_MAX));
}

static int add_lang(const char *lang, const char *locale_name) {
    struct SelvaLang *slang;
    int err;

    slang = selva_calloc(1, sizeof(*slang));
    slang->locale = newlocale(LC_ALL_MASK, locale_name, 0);
    if (!slang->locale) {
        if (errno == EINVAL) {
            err = SELVA_EINVAL;
        } else if (errno == ENOENT) {
            err = SELVA_ENOENT;
        } else if (errno == ENOMEM) {
            err = SELVA_ENOMEM;
        } else {
            err = SELVA_EGENERAL;
        }

        return err;
    }

    /*
     * Note that slang->name is not supposed to be nul-terminated.
     */
    strncpy(slang->name, lang, sizeof(slang->name));
    get_territory(slang->territory, locale_name);

    err = SelvaObject_SetPointerStr(langs, lang, strnlen(lang, LANG_NAME_MAX), slang, &obj_opts);
    if (err) {
        selva_free(slang);
    }

    return err;
}

static void SelvaLang_Free(void *p) {
    struct SelvaLang *slang = (struct SelvaLang *)p;

    freelocale(slang->locale);
    selva_free(slang);
}

locale_t SelvaLang_GetLocale(const char *lang_str, size_t lang_len) {
    struct SelvaLang *slang;
    int err = SELVA_EINVAL;

    if (lang_len > 0) {
        void *p;

        err = SelvaObject_GetPointerStr(langs, lang_str, lang_len, &p);
        slang = p;
    }
    if (err) {
        void *p;

        if (lang_len > 0) {
            SELVA_LOG(SELVA_LOGL_ERR, "Lang \"%.*s\" not found: %s\n",
                      (int)lang_len, lang_str,
                      getSelvaErrorStr(err));
        }

        err = SelvaObject_GetPointerStr(langs, FALLBACK_LANG, sizeof(FALLBACK_LANG) - 1, &p);
        slang = p;
        if (err) {
            return duplocale(LC_GLOBAL_LOCALE); /* Finally fallback to the global locale. */
        }
    }

    return slang->locale;
}

static void load_lang(const char *lang, const char *locale_name) {
    int err;

    err = add_lang(lang, locale_name);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Loading locale %s for lang %s failed with error: %s\n",
                locale_name, lang,
                getSelvaErrorStr(err));
    }
}

#define LOAD_LANG(lang, loc_lang) \
    load_lang(#lang, #loc_lang ".UTF-8");

int SelvaLang_ListCommand(RedisModuleCtx *ctx, RedisModuleString **argv __unused, int argc __unused) {
    RedisModule_AutoMemory(ctx);

    if (argc != 1) {
        return RedisModule_WrongArity(ctx);
    }

    return SelvaObject_ReplyWithObject(ctx, NULL, langs, NULL, 0);
}

static void SelvaLang_Reply(struct RedisModuleCtx *ctx, void *p) {
    const struct SelvaLang *slang = (struct SelvaLang *)p;

    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithStringBuffer(ctx, slang->name, strnlen(slang->name, LANG_NAME_MAX));
    RedisModule_ReplyWithStringBuffer(ctx, slang->territory, strnlen(slang->territory, LANG_TERRITORY_MAX));
}

static int SelvaLang_OnLoad(RedisModuleCtx *ctx __unused) {
    langs = SelvaObject_New();

    FORALL_LANGS(LOAD_LANG)

    if (RedisModule_CreateCommand(ctx, "selva.lang.list", SelvaLang_ListCommand, "readonly allow-loading", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return 0;
}
SELVA_ONLOAD(SelvaLang_OnLoad);

static void SelvaLang_OnUnload(void) {
    /*
     * We could free the langs here but the glibc locale system seems to leak
     * memory anyway, so why bother. All memory will get eventually freed when
     * Redis finally exits.
     */
#if 0
    SelvaObject_Destroy(langs);
#endif
}
SELVA_ONUNLOAD(SelvaLang_OnUnload);
