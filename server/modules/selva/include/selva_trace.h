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

/**
 * Create a new tracing handle.
 * This is needed for every trace, SELVA_TRACE_BEGIN and SELVA_TRACE_END paid.
 */
#define SELVA_TRACE_HANDLE(_name) \
   struct SelvaTrace CONCATENATE(selva_trace_handle_, _name) = { \
       .name = #_name, \
   }; \
   DATA_SET(selva_trace, CONCATENATE(selva_trace_handle_, _name))

/**
 * Begin a new trace.
 * Traces can be nested but different traces will appear flat in the analysis
 * results. Bear in mind that traces are stacked so an outter trace cannot be
 * terminated before its inner trace.
 */
#define SELVA_TRACE_BEGIN(_name) \
    __itt_task_begin(selva_trace_domain, __itt_null, __itt_null, CONCATENATE(selva_trace_handle_, _name).handle)

/**
 * Begin a new automatic trace.
 * The trace is automatically terminated when the block scope ends.
 * SELVA_TRACE_END() must not be called for an automatic trace.
 */
#define SELVA_TRACE_BEGIN_AUTO(_name) \
    __attribute__((cleanup(SelvaTrace_AutoEnd))) const struct SelvaTrace *CONCATENATE(_autoend_selva_trace_, _name) = \
        &CONCATENATE(selva_trace_handle_, _name); \
    __itt_task_begin(selva_trace_domain, __itt_null, __itt_null, CONCATENATE(selva_trace_handle_, _name).handle)

/**
 * End a trace.
 */
#define SELVA_TRACE_END(_name) \
    CONCATENATE(selva_trace_handle_, _name); \
    __itt_task_end(selva_trace_domain)

void SelvaTrace_AutoEnd(void *);

/**
 * The Selva tracing domain.
 */
extern __itt_domain* selva_trace_domain;

#else
#define SELVA_TRACE_HANDLE(name)
#define SELVA_TRACE_BEGIN(name)
#define SELVA_TRACE_END(name)
#define SELVA_TRACE_BEGIN_AUTO(_name)
#endif

#endif /* _SELVA_TRACE_H_ */
