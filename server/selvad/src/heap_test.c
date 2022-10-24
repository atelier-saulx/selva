#include <stddef.h>
#include <stdio.h>
#include "heap.h"

#define SIZE 10

struct s {
    int data[SIZE];
    HEAP_DEF(my, SIZE);
};

int cmp(const void *data, heap_value_t a, heap_value_t b)
{
    int *d = (int *)data;

    return d[a] - d[b];
}

void print_heap(struct heap *heap)
{

    printf("{ ");
    if (heap->last >= 0) {
        printf("%d", heap->a[0]);
    }
    for (size_t i = 1; i <= heap->last; i++) {
        printf(", %d", heap->a[i]);
    }
    printf(" }\n");
}

int main(void)
{
    struct s s = {
        .data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
    };

    heap_init(&s.my, s.data, cmp, SIZE);

    printf("data: %p\nheap_arr: %p\nact_harr: %p\n",
           s.data,
           s.my.a,
           s.my_arr
          );

    heap_insert(&s.my, 1);
    heap_insert(&s.my, 7);
    print_heap(&s.my);
    printf("root: %d\n", heap_del_max(&s.my));
    print_heap(&s.my);

    return 0;
}
