#include <punit.h>
#include <stdio.h>
#include "cdefs.h"
#include "util/eztrie.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static void print_eztrie(struct eztrie_iterator it)
{
    struct eztrie_node_value * value;

    while ((value = eztrie_remove_ithead(&it))) {
        printf("key: %s, value: %d\n", value->key, *((int *)value->p));
    }
    printf("\n");
}

static char * test_destroy(void)
{
    struct eztrie trie;

    eztrie_init(&trie);
    eztrie_destroy(&trie, NULL, NULL);

    return NULL;
}

static char * test_insert(void)
{
    struct eztrie trie;
    int x = 1, y = 2;
    struct eztrie_iterator it;
    struct eztrie_node_value *res;

    eztrie_init(&trie);

    eztrie_insert(&trie, "abc", &x);
    eztrie_insert(&trie, "abcd", &y);

    it = eztrie_find(&trie, "ab");

    res = eztrie_remove_ithead(&it);
    pu_assert_str_equal("", res->key, "abc");
    pu_assert_ptr_equal("", res->p, &x);

    res = eztrie_remove_ithead(&it);
    pu_assert_str_equal("", res->key, "abcd");
    pu_assert_ptr_equal("", res->p, &y);

    eztrie_destroy(&trie, NULL, NULL);

    return NULL;
}

static char * test_remove(void)
{
    struct eztrie trie;
    int x = 1, y = 2;
    struct eztrie_iterator it;
    struct eztrie_node_value *res;

    eztrie_init(&trie);

    eztrie_insert(&trie, "abc", &x);
    eztrie_insert(&trie, "abcd", &y);

    eztrie_remove(&trie, "abc");

    it = eztrie_find(&trie, "ab");

    res = eztrie_remove_ithead(&it);
    pu_assert_str_equal("", res->key, "abcd");
    pu_assert_ptr_equal("", res->p, &y);

    eztrie_destroy(&trie, NULL, NULL);

    return NULL;
}

static void my_cb(void *p, void *arg)
{
    (*(int *)arg)++;
}

static char * test_destroy_cb(void)
{
    struct eztrie trie;
    int arr[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    struct eztrie_iterator it;
    struct eztrie_node_value *res;
    int count = 0;

    eztrie_init(&trie);

    eztrie_insert(&trie, "a", arr + 0);
    eztrie_insert(&trie, "ab", arr + 1);
    eztrie_insert(&trie, "abc", arr + 2);
    eztrie_insert(&trie, "abcd", arr + 3);
    eztrie_insert(&trie, "z", arr + 4);
    eztrie_insert(&trie, "zy", arr + 5);
    eztrie_insert(&trie, "zyz", arr + 6);
    eztrie_insert(&trie, "zxy", arr + 7);
    eztrie_insert(&trie, "12", arr + 8);
    eztrie_insert(&trie, "132", arr + 9);

    eztrie_destroy(&trie, my_cb, &count);
    pu_assert_equal("Called cb for every item", count, num_elem(arr));

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_destroy, PU_RUN);
    pu_def_test(test_insert, PU_RUN);
    pu_def_test(test_remove, PU_RUN);
    pu_def_test(test_destroy_cb, PU_RUN);
}
