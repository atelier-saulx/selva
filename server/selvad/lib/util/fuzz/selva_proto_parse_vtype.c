#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include "selva_proto.h"

void *selva_malloc(size_t n)
{
    return malloc(n);
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    const uint8_t *buf;
    size_t bsize;
    size_t i;
    enum selva_proto_data_type type_out;
    size_t len_out;

    if (Size < sizeof(i)) {
        return 0;
    }

    buf = Data + sizeof(i);
    bsize = Size - sizeof(i);
    memcpy(&i, Data, sizeof(i));

    (void)selva_proto_parse_vtype(buf, bsize, i, &type_out, &len_out)

    return 0;
}
