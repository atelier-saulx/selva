/**
 *******************************************************************************
 * @file    heap.h
 * @author  Olli Vanhoja
 *
 * @section LICENSE
 * Copyright (c) 2022 Saulx
 * Copyright (c) 2013 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * Copyright (c) 2012, 2013, Ninjaware Oy, Olli Vanhoja <olli.vanhoja@ninjaware.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

#pragma once

typedef short heap_value_t;
typedef int (*heap_comp)(const void *data, heap_value_t a, heap_value_t b);

/**
 * Heap data struct.
 */
struct heap {
    const void *data;
    heap_comp cmp; /*!< Compare two items. */
    int size; /*!< Max heap size. */
    int last; /*!< Current heap size. (last item)  */
    heap_value_t a[0]; /*!< Heap array. */
} __attribute__((packed, aligned(sizeof(void *))));

/**
 * Define a heap in a struct.
 */
#define HEAP_DEF(_heap_name, _heap_size) \
    struct heap _heap_name; \
    heap_value_t CONCATENATE(_heap_name, _arr)[_heap_size] __attribute__((aligned(sizeof(void *))))

static inline void heap_init(struct heap *h, const void *data, heap_comp cmp, size_t size)
{
    h->data = data;
    h->cmp = cmp;
    h->size = size;
    h->last = -1;
}

/**
 * Get the current length of the heap.
 */
static inline int heap_len(struct heap * heap)
{
    return heap->last + 1;
}

/**
 * Get the current max value of the heap.
 */
static inline heap_value_t heap_peek_max(struct heap * heap)
{
    return heap->last >= 0 ? heap->a[0] : -1;
}

/**
 * Removes the thread on top of a heap.
 * @param heap is a pointer to the heap struct.
 */
heap_value_t heap_del_max(struct heap * heap);

/**
 * Insert a new element to the heap.
 * @param heap is a pointer to the heap struct.
 */
int heap_insert(struct heap * heap, heap_value_t k);

/**
 * Heap increment key.
 * @note Parameters are not asserted. If key is not actually greater than it
 * previously was this operation might not work as expected.
 * @param heap is a pointer to the heap struct.
 */
void heap_inc_key(struct heap * heap, heap_value_t k);

/**
 * Heap decrement key.
 * @note Parameters are not asserted. If key is not actually smaller than it
 * previously was this operation might not work as expected.
 * @param heap is a pointer to the heap struct.
 */
void heap_dec_key(struct heap * heap, heap_value_t k);

/**
 * Reschedule the root of the heap.
 * @param heap is a pointer to the heap struct.
 */
void heap_reschedule_root(struct heap * heap);
