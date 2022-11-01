/**
 *******************************************************************************
 * @file    heap.c
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
 * SPDX-License-Identifier: BSD-2-Clause
 *******************************************************************************
 */

#include <stddef.h>
#include "heap.h"

#if !defined(__clang__)
/*
 * Proper alignment test.
 */
struct _heap_assert {
    HEAP_DEF(h, 2);
};
static struct _heap_assert _heap_assert;
static_assert(_heap_assert.h.a == _heap_assert.h_arr, "Invalid heap struct alignment");
#endif

/**
 * Returns index to the parent of the key i.
 * @param i Index to a key.
 */
static inline int parent(int i)
{
    return i / 2;
}

/**
 * Returns index to the key on the left side of the key i.
 * @param i Index to a key.
 */
static inline int left(int i)
{
    return 2 * i;
}

/**
 * Returns index to the key on the right side of the key i.
 * @param i Index to a key.
 */
static inline int right(int i)
{
    return 2 * i + 1;
}

/**
 * Swap two elements in a heap.
 * @param heap is a pointer to the heap struct.
 * @param i Index in heap array.
 * @param j Index in heap array.
 */
static inline void swap(struct heap * heap, int i, int j)
{
    heap_value_t temp;

    temp = heap->a[i];
    heap->a[i] = heap->a[j];
    heap->a[j] = temp;
}

/**
 * Fix the heap.
 * @param heap is a pointer to the heap struct.
 * @param i is the current index in the heap array.
 */
static void heapify(struct heap * heap, int i)
{
    int l = left(i);
    int r = right(i);

    if (r <= heap->last) {
        int largest;

        if (heap->cmp(heap->data, heap->a[i], heap->a[r]) > 0) {
            largest = l;
        } else {
            largest = r;
        }

        if (heap->cmp(heap->data, heap->a[i], heap->a[largest]) < 0) {
            swap(heap, i, largest);
            heapify(heap, largest);
        }
    } else if ((l == heap->last) && heap->cmp(heap->data, heap->a[i], heap->a[l]) < 0) {
        swap(heap, i, l);
    }
}

heap_value_t heap_del_max(struct heap * heap)
{
    heap_value_t root = -1;

    if (heap->last >= 0) {
        root = heap->a[0];
        heap->a[0] = heap->a[heap->last];
        heap->last--;
        heapify(heap, 0);
    }

    return root;
}

int heap_insert(struct heap * heap, heap_value_t k)
{
    int i;

    if (heap->last == heap->size - 1) {
        return -1;
    }

    i = ++heap->last;
    while ((i > 0) && heap->cmp(heap->data, heap->a[parent(i)], k) < 0) {
        heap->a[i] = heap->a[parent(i)];
        i = parent(i);
    }
    heap->a[i] = k;

    return 0;
}

/**
 * Find thread from a heap array.
 * @param heap is a pointer to the heap struct.
 */
static int heap_find(struct heap * heap, heap_value_t k)
{
    for (int i = 0; i <= heap->last; i++) {
        if (heap->a[i] == k) {
            return i;
        }
    }

    return -1;
}

void heap_inc_key(struct heap * heap, heap_value_t k)
{
    int i = heap_find(heap, k);

    while ((i > 0) && heap->cmp(heap->data, heap->a[parent(i)], heap->a[i]) < 0) {
        swap(heap, i, parent(i));
        i = parent(i);
    }
}

void heap_dec_key(struct heap * heap, heap_value_t k)
{
    /*
     * The data element must be updated before calling this function.
     */
    heapify(heap, heap_find(heap, k));
}

void heap_reschedule_root(struct heap * heap)
{
    int s = heap->last;

    /* Swap root with the last item */
    swap(heap, 0, s);
    heap->last--;
    heapify(heap, 0);
    heap->last++;

    /* Move upwards */
    while ((s > 0) && heap->cmp(heap->data, heap->a[parent(s)], heap->a[s]) <= 0) {
        swap(heap, s, parent(s));
        s = parent(s);
    }
}
