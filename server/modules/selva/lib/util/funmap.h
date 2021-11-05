#pragma once
#ifndef _FUNMAP_H_
#define _FUNMAP_H_

#define GENERATE_FUNMAP_PROTO(name, funcs, selector_type) \
    typeof(*(funcs)) name(selector_type v);

#define GENERATE_STATIC_FUNMAP_PROTO(name, funcs, selector_type) \
    static GENERATE_FUNMAP_PROTO(name, funcs, selector_type);

#define GENERATE_FUNMAP(name, funcs, selector_type, default_index) \
    typeof(*(funcs)) name(selector_type v) { \
        return funcs[((long unsigned int)v >= (sizeof(funcs) / sizeof(*(funcs)))) \
            ? (long unsigned int)(default_index) : (long unsigned int)v]; \
    }

#define GENERATE_STATIC_FUNMAP(name, funcs, selector_type, default_index) \
    static GENERATE_FUNMAP(name, funcs, selector_type, default_index)

#endif /* _FUNMAP_H_ */
