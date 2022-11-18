/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "selva_log.h"
#include "module.h"
#include "selva_io.h"

void selva_io_save_unsigned(struct selva_io *io, uint64_t value)
{
}

void selva_io_save_signed(struct selva_io *io, int64_t value)
{
}

void selva_io_save_float(struct selva_io *io, float f)
{
}

void selva_io_save_double(struct selva_io *io, double d)
{
}

void selva_io_save_long_double(struct selva_io *io, long double ld)
{
}

void selva_io_save_str(struct selva_io *io, const char *str, size_t len)
{
}

void selva_io_save_string(struct selva_io *io, const struct selva_string *s)
{
}

uint64_t selva_io_load_unsigned(struct selva_io *io)
{
    return 0;
}

int64_t selva_io_load_signed(struct selva_io *io)
{
    return 0;
}

float selva_io_save_load_float(struct selva_io *io)
{
    return 0.0f;
}

double selva_io_save_load_double(struct selva_io *io)
{
    return 0.0;
}

long double selva_io_save_load_long_double(struct selva_io *io)
{
    return (long double)0.0;
}

const char *selva_io_load_str(struct selva_io *io, size_t *len)
{
    return NULL;
}

const struct selva_string *selva_io_load_string(struct selva_io *io)
{
    return NULL;
}

IMPORT() {
    evl_import_main(selva_log);
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init io");
}
