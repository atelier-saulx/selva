#pragma once
#ifndef _UTIL_CDEFS_H_
#define _UTIL_CDEFS_H_

#if defined(__GNUC__) && !defined(__clang__)
#define __nonstring __attribute__((nonstring))
#else
#define __nonstring
#endif

#define CONCATENATE(arg1, arg2)   CONCATENATE1(arg1, arg2)
#define CONCATENATE1(arg1, arg2)  CONCATENATE2(arg1, arg2)
#define CONCATENATE2(arg1, arg2)  arg1##arg2

#define UTIL_NARG(...) \
    UTIL_NARG_(__VA_ARGS__, UTIL_RSEQ_N())
#define UTIL_NARG_(...) UTIL_ARG_N(__VA_ARGS__)
#define UTIL_ARG_N( \
     _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, _10, _11, _12, _13, _14, \
    _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, \
    _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, \
    _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, \
    _57, _58, _59, _60, _61, _62, _63, N, ...) N
# define UTIL_RSEQ_N() \
    63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, \
    46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
    29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, \
    12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0

#define containerof(x, s, m) ({                     \
        const __typeof(((s *)0)->m) *__x = (x);     \
        ((s *)((uint8_t *)(__x) - offsetof(s, m))); \
})

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifndef __GLOBL1
#define  __GLOBL1(sym) __asm__(".globl " #sym)
#define  __GLOBL(sym) __GLOBL1(sym)
#endif

#ifndef __used
#define __used __attribute__((__used__))
#endif

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#ifndef __section
#define __section(x) __attribute__((__section__(x)))
#endif

#define __constructor   __attribute__((constructor))
#define __destructor    __attribute__((destructor))

#define num_elem(x) (sizeof(x) / sizeof(*(x)))

#define min(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b; })

#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a > _b ? _a : _b; })


#define TO_STR_1(_var) \
    size_t _var##_len; \
    const char * _var##_str = RedisModule_StringPtrLen(_var, & _var##_len);

#define TO_STR_2(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_1(__VA_ARGS__)

#define TO_STR_3(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_2(__VA_ARGS__)

#define TO_STR_4(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_3(__VA_ARGS__)

#define TO_STR_5(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_5(__VA_ARGS__)

#define TO_STR_6(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_6(__VA_ARGS__)

#define TO_STR(...) \
        CONCATENATE(TO_STR_, UTIL_NARG(__VA_ARGS__))(__VA_ARGS__)

#endif /* _UTIL_CDEFS_H_ */
