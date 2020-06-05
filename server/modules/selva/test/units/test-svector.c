#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "svector.h"
#include "cdefs.h"

struct data {
    int id;
};

struct Vector vec;

static int compar(const void ** restrict ap, const void ** restrict bp)
{
    const struct data *a = *(const struct data **)ap;
    const struct data *b = *(const struct data **)bp;

    return a->id - b->id;
}

static void setup(void)
{
    memset(&vec, 0, sizeof(struct Vector));
}

static void teardown(void)
{
    free(vec.vec_data);
}

static char * test_init_works(void)
{
    Vector *vecP = Vector_Init(&vec, 100, compar);

    pu_assert_ptr_equal("the vector is returned", vecP, &vec);
    pu_assert_ptr_equal("compar is set", vec.vec_compar, compar);
    pu_assert_equal("length is correct", vec.vec_len, 100);
    pu_assert_equal("last is zeroed", vec.vec_last, 0);

    return NULL;
}

static char * test_can_destroy(void)
{
    Vector *vecP = Vector_Init(&vec, 100, compar);

    Vector_Destroy(vecP);
    // multiple times
    Vector_Destroy(vecP);

    return NULL;
}

static char * test_insert_one(void)
{
    struct data el1 = {
        .id = 10,
    };

    Vector_Init(&vec, 5, compar);
    Vector_Insert(&vec, &el1);

    pu_assert_equal("last is incremented", vec.vec_last, 1);
    pu_assert_ptr_equal("el1 was inserted", vec.vec_data[0], &el1);

    return NULL;
}

static char * test_insert_two_desc(void)
{
    struct data el1 = {
        .id = 10,
    };
    struct data el2 = {
        .id = 1,
    };

    Vector_Init(&vec, 5, compar);
    Vector_Insert(&vec, &el1);
    Vector_Insert(&vec, &el2);

    pu_assert_equal("last is incremented", vec.vec_last, 2);
    pu_assert_ptr_equal("el1 was inserted correctly", vec.vec_data[1], &el1);
    pu_assert_ptr_equal("el2 was inserted correctly", vec.vec_data[0], &el2);

    return NULL;
}

static char * test_insert_many(void)
{
    struct data el[] = { { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 } };

    Vector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        Vector_Insert(&vec, &el[i]);
    }

    pu_assert_equal("last is incremented", vec.vec_last, 8);

    struct data **data = ((struct data **)vec.vec_data);
    pu_assert_ptr_equal("el was inserted correctly", data[0]->id, 1);
    pu_assert_ptr_equal("el was inserted correctly", data[1]->id, 3);
    pu_assert_ptr_equal("el was inserted correctly", data[2]->id, 5);
    pu_assert_ptr_equal("el was inserted correctly", data[3]->id, 10);
    pu_assert_ptr_equal("el was inserted correctly", data[4]->id, 15);
    pu_assert_ptr_equal("el was inserted correctly", data[5]->id, 20);
    pu_assert_ptr_equal("el was inserted correctly", data[6]->id, 300);
    pu_assert_ptr_equal("el was inserted correctly", data[7]->id, 800);

    return NULL;
}

static char * test_search(void)
{
    struct data el[] = { { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 } };

    Vector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        Vector_Insert(&vec, &el[i]);
    }

    const struct data *res = Vector_Search(&vec, &(struct data){ 15 });

    pu_assert_ptr_equal("found the right one", res, &el[2]);

    return NULL;
}

static char * test_remove_one(void)
{
    struct data el1 = {
        .id = 10,
    };

    Vector_Init(&vec, 5, compar);
    Vector_Insert(&vec, &el1);
    Vector_Remove(&vec, &el1);

    pu_assert_equal("last is zeroed", vec.vec_last, 0);

    return NULL;
}

static char * test_remove_one_compound_literal(void)
{
    struct data el1 = {
        .id = 10,
    };

    Vector_Init(&vec, 5, compar);
    Vector_Insert(&vec, &el1);
    struct data *rem = Vector_Remove(&vec, &(struct data){ 10 });

    pu_assert_equal("last is zeroed", vec.vec_last, 0);
    pu_assert_ptr_equal("the removed item was returned", rem, &el1);

    return NULL;
}

static char * test_remove_last(void)
{
    struct data el1 = {
        .id = 1,
    };
    struct data el2 = {
        .id = 2,
    };

    Vector_Init(&vec, 3, compar);
    Vector_Insert(&vec, &el1);
    Vector_Insert(&vec, &el2);
    Vector_Remove(&vec, &el2);

    pu_assert_equal("last is decremented", vec.vec_last, 1);
    pu_assert_ptr_equal("el1 was is still there", vec.vec_data[0], &el1);

    return NULL;
}

static char * test_remove_first(void)
{
    struct data el1 = {
        .id = 1,
    };
    struct data el2 = {
        .id = 2,
    };

    Vector_Init(&vec, 3, compar);
    Vector_Insert(&vec, &el1);
    Vector_Insert(&vec, &el2);
    Vector_Remove(&vec, &el1);

    pu_assert_equal("last is decremented", vec.vec_last, 1);
    pu_assert_ptr_equal("el2 was is still there", vec.vec_data[0], &el2);

    return NULL;
}

static char * test_remove_middle(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    Vector_Init(&vec, 3, compar);
    for (size_t i = 0; i < num_elem(el); i++) {
        Vector_Insert(&vec, &el[i]);
    }

    Vector_Remove(&vec, &(struct data){ 2 });

    pu_assert_equal("last is decremented", vec.vec_last, 2);
    pu_assert_ptr_equal("el[0] was is still there", vec.vec_data[0], &el[0]);
    pu_assert_ptr_equal("el[2] was is still there", vec.vec_data[1], &el[2]);

    return NULL;
}

static char * test_pop(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    Vector_Init(&vec, 3, NULL);
    for (size_t i = 0; i < num_elem(el); i++) {
        Vector_Insert(&vec, &el[i]);
    }

    pu_assert_ptr_equal("Pops el[2]", Vector_Pop(&vec), &el[2]);
    pu_assert_ptr_equal("Pops el[1]", Vector_Pop(&vec), &el[1]);

    Vector_Insert(&vec, &el[0]);
    pu_assert_ptr_equal("Pops el[0]", Vector_Pop(&vec), &el[0]);
    pu_assert_ptr_equal("Pops el[0]", Vector_Pop(&vec), &el[0]);
    pu_assert_equal("Vector size is zeroed", Vector_Size(&vec), 0);

    return NULL;
}

static char * test_foreach(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    Vector_Init(&vec, 3, compar);
    for (size_t i = 0; i < num_elem(el); i++) {
        Vector_Insert(&vec, &el[i]);
    }

    size_t i = 0;
    struct data *d;
    /* cppcheck-suppress internalAstError */
    VECTOR_FOREACH(d, &vec) {
        pu_assert_ptr_equal("el[0] is pointing to the correct item", d, &el[i++]);
    }

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_init_works, PU_RUN);
    pu_def_test(test_can_destroy, PU_RUN);
    pu_def_test(test_insert_one, PU_RUN);
    pu_def_test(test_insert_two_desc, PU_RUN);
    pu_def_test(test_insert_many, PU_RUN);
    pu_def_test(test_search, PU_RUN);
    pu_def_test(test_remove_one, PU_RUN);
    pu_def_test(test_remove_one_compound_literal, PU_RUN);
    pu_def_test(test_remove_last, PU_RUN);
    pu_def_test(test_remove_first, PU_RUN);
    pu_def_test(test_remove_middle, PU_RUN);
    pu_def_test(test_pop, PU_RUN);
    pu_def_test(test_foreach, PU_RUN);
}
