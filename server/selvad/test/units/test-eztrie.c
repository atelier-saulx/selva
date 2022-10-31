#include <punit.h>
#include <stdio.h>
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
    eztrie_destroy(&trie, NULL);

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

    eztrie_destroy(&trie, NULL);

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

    eztrie_destroy(&trie, NULL);

    return NULL;
}

/* TODO Test free callback. */

void all_tests(void)
{
    pu_def_test(test_destroy, PU_RUN);
    pu_def_test(test_insert, PU_RUN);
    pu_def_test(test_remove, PU_RUN);
}
