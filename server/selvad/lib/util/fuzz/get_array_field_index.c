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
    ssize_t res;

    (void)get_array_field_index(Data, Size, &res);

    return 0;
}
