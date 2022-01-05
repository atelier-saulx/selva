#include "selva_trace.h"

/*
 * The initialization code needs to be compiled only if tracing is enabled.
 */
#ifdef SELVA_TRACE
SET_DECLARE(selva_trace, struct SelvaTrace);
__itt_domain* selva_trace_domain;

__constructor static void init_selva_trace(void) {
    struct SelvaTrace **data_p;

    selva_trace_domain = __itt_domain_create("Selva.Domain.Global");

    SET_FOREACH(data_p, selva_trace) {
        struct SelvaTrace *data = *data_p;

        data->handle = __itt_string_handle_create(data->name);
    }
}

void SelvaTrace_AutoEnd(void *p __unused) {
    __itt_task_end(selva_trace_domain);
}
#endif
