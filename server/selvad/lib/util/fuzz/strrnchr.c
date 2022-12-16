// Copyright (c) 2022 SAULX
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include "util/cstrings.h"

void *selva_malloc(size_t n)
{
    return malloc(n);
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Size < 1) {
        return 0;
    }

    (void)strrnchr(Data + 1, Size - 1, Data[0]);

    return 0;
}
