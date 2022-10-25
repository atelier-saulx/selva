/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#if EVL_MAIN
#define EVL_EXPORT(_ret_, _fun_name_, ...) _ret_ _fun_name_(__VA_ARGS__) __attribute__((__visibility__("default")))
#else
#define EVL_EXPORT(_ret_, _fun_name_, ...) _ret_ (*_fun_name_)(__VA_ARGS__) __attribute__((__common__))
#endif
