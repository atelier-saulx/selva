/*
 * Selva IO Module.
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

#if SELVA_IO_MAIN
#define SELVA_IO_EXPORT(_ret_, _fun_name_, ...) _ret_ _fun_name_(__VA_ARGS__) EVL_EXTERN
#else
#define SELVA_IO_EXPORT(_ret_, _fun_name_, ...) _ret_ (*_fun_name_)(__VA_ARGS__) EVL_COMMON
#endif

struct selva_io;
struct selva_string;

SELVA_IO_EXPORT(void, selva_io_save_unsigned, struct selva_io *io, uint64_t value);
SELVA_IO_EXPORT(void, selva_io_save_signed, struct selva_io *io, int64_t value);
SELVA_IO_EXPORT(void, selva_io_save_float, struct selva_io *io, float f);
SELVA_IO_EXPORT(void, selva_io_save_double, struct selva_io *io, double d);
SELVA_IO_EXPORT(void, selva_io_save_long_double, struct selva_io *io, long double ld);
SELVA_IO_EXPORT(void, selva_io_save_str, struct selva_io *io, const char *str, size_t len);
SELVA_IO_EXPORT(void, selva_io_save_string, struct selva_io *io, const struct selva_string *s);

SELVA_IO_EXPORT(uint64_t, selva_io_load_unsigned, struct selva_io *io);
SELVA_IO_EXPORT(int64_t, selva_io_load_signed, struct selva_io *io);
SELVA_IO_EXPORT(float, selva_io_load_float, struct selva_io *io);
SELVA_IO_EXPORT(double, selva_io_load_double, struct selva_io *io);
SELVA_IO_EXPORT(long double, selva_io_load_long_double, struct selva_io *io);
SELVA_IO_EXPORT(const char*, selva_io_load_str, struct selva_io *io, size_t *len);
SELVA_IO_EXPORT(const struct selva_string *, selva_io_load_string, struct selva_io *io);
