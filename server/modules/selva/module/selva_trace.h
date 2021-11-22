#pragma once
#ifndef _SELVA_TRACE_H_

#include "cdefs.h"
#include "linker_set.h"

#ifdef SELVA_TRACE
#include <ittnotify.h>
#endif

struct SelvaTrace {
#ifdef SELVA_TRACE
    __itt_string_handle* handle;
    const char *name;
#endif
};

#ifdef SELVA_TRACE

#define SELVA_TRACE_HANDLE(_name) \
   struct SelvaTrace CONCATENATE(selva_trace_handle_, _name) = { \
       .name = #_name, \
   }; \
   DATA_SET(selva_trace, CONCATENATE(selva_trace_handle_, _name))

#define SELVA_TRACE_BEGIN(name) \
    __itt_task_begin(selva_trace_domain, __itt_null, __itt_null, CONCATENATE(selva_trace_handle_, name).handle);

#define SELVA_TRACE_END(name) \
    __itt_task_end(selva_trace_domain)

extern __itt_domain* selva_trace_domain;

#else
#define SELVA_TRACE_HANDLE(name)
#define SELVA_TRACE_BEGIN(name)
#define SELVA_TRACE_END(name)
#endif

#endif /* _SELVA_TRACE_H_ */
