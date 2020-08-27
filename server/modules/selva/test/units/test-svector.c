#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "svector.h"
#include "cdefs.h"

struct data {
    int id;
};

struct SVector vec;

static int compar(const void ** restrict ap, const void ** restrict bp)
{
    const struct data *a = *(const struct data **)ap;
    const struct data *b = *(const struct data **)bp;

    return a->id - b->id;
}

static void setup(void)
{
    memset(&vec, 0, sizeof(struct SVector));
}

static void teardown(void)
{
    free(vec.vec_data);
}

static char * test_init_works(void)
{
    SVector *vecP = SVector_Init(&vec, 100, compar);

    pu_assert_ptr_equal("the vector is returned", vecP, &vec);
    pu_assert_ptr_equal("compar is set", vec.vec_compar, compar);
    pu_assert_equal("length is correct", vec.vec_len, 100);
    pu_assert_equal("last is zeroed", vec.vec_last, 0);

    return NULL;
}

static char * test_can_destroy(void)
{
    SVector *vecP = SVector_Init(&vec, 100, compar);

    SVector_Destroy(vecP);
    // multiple times
    SVector_Destroy(vecP);

    return NULL;
}

static char * test_insert_one(void)
{
    struct data el1 = {
        .id = 10,
    };

    SVector_Init(&vec, 5, compar);
    SVector_Insert(&vec, &el1);

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

    SVector_Init(&vec, 5, compar);
    SVector_Insert(&vec, &el1);
    SVector_Insert(&vec, &el2);

    pu_assert_equal("last is incremented", vec.vec_last, 2);
    pu_assert_ptr_equal("el1 was inserted correctly", vec.vec_data[1], &el1);
    pu_assert_ptr_equal("el2 was inserted correctly", vec.vec_data[0], &el2);

    return NULL;
}

static char * test_insert_many(void)
{
    struct data el[] = { { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 } };

    SVector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
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

static char * test_insertFast_many(void)
{
    struct data el[] = { { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 } };

    SVector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        const void * r = SVector_InsertFast(&vec, &el[i]);
        pu_assert_ptr_equal("No return value", r, NULL);
    }

    pu_assert_equal("last is incremented", vec.vec_last, 8);

    struct data **data = ((struct data **)vec.vec_data);
    pu_assert_ptr_equal("el[0] was inserted correctly", data[0]->id, 1);
    pu_assert_ptr_equal("el[1] was inserted correctly", data[1]->id, 3);
    pu_assert_ptr_equal("el[2] was inserted correctly", data[2]->id, 5);
    pu_assert_ptr_equal("el[3] was inserted correctly", data[3]->id, 10);
    pu_assert_ptr_equal("el[4] was inserted correctly", data[4]->id, 15);
    pu_assert_ptr_equal("el[5] was inserted correctly", data[5]->id, 20);
    pu_assert_ptr_equal("el[6] was inserted correctly", data[6]->id, 300);
    pu_assert_ptr_equal("el[7] was inserted correctly", data[7]->id, 800);

    return NULL;
}

static char * test_insertFast_dedup(void)
{
    struct data el1 = {
        .id = 10,
    };

    SVector_Init(&vec, 5, compar);
    const void *r1 = SVector_InsertFast(&vec, &el1);
    const void *r2 = SVector_InsertFast(&vec, &el1);

    pu_assert_equal("last is incremented", vec.vec_last, 1);
    pu_assert_ptr_equal("el1 was inserted", vec.vec_data[0], &el1);
    pu_assert_ptr_equal("r1 = NULL", r1, NULL);
    pu_assert_ptr_equal("r2 = el1", r2, &el1);

    return NULL;
}

static char * test_insert_no_compar(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    SVector_Init(&vec, 3, NULL);
    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    pu_assert_equal("last is incremented", vec.vec_last, 3);
    pu_assert_ptr_equal("el[0] was inserted correctly", vec.vec_data[0], &el[0]);
    pu_assert_ptr_equal("el[1] was inserted correctly", vec.vec_data[1], &el[1]);
    pu_assert_ptr_equal("el[2] was inserted correctly", vec.vec_data[2], &el[2]);

    return NULL;
}

static char * test_search(void)
{
    struct data el[] = { { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 } };

    SVector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    const struct data *res = SVector_Search(&vec, &(struct data){ 15 });

    pu_assert_ptr_equal("found the right one", res, &el[2]);

    return NULL;
}

static char * test_remove_one(void)
{
    struct data el1 = {
        .id = 10,
    };

    SVector_Init(&vec, 5, compar);
    SVector_Insert(&vec, &el1);
    SVector_Remove(&vec, &el1);

    pu_assert_equal("last is zeroed", vec.vec_last, 0);

    return NULL;
}

static char * test_remove_one_compound_literal(void)
{
    struct data el1 = {
        .id = 10,
    };

    SVector_Init(&vec, 5, compar);
    SVector_Insert(&vec, &el1);
    struct data *rem = SVector_Remove(&vec, &(struct data){ 10 });

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

    SVector_Init(&vec, 3, compar);
    SVector_Insert(&vec, &el1);
    SVector_Insert(&vec, &el2);
    SVector_Remove(&vec, &el2);

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

    SVector_Init(&vec, 3, compar);
    SVector_Insert(&vec, &el1);
    SVector_Insert(&vec, &el2);
    SVector_Remove(&vec, &el1);

    pu_assert_equal("last is decremented", vec.vec_last, 1);
    pu_assert_ptr_equal("el2 was is still there", vec.vec_data[0], &el2);

    return NULL;
}

static char * test_remove_middle(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    SVector_Init(&vec, 3, compar);
    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    SVector_Remove(&vec, &(struct data){ 2 });

    pu_assert_equal("last is decremented", vec.vec_last, 2);
    pu_assert_ptr_equal("el[0] was is still there", vec.vec_data[0], &el[0]);
    pu_assert_ptr_equal("el[2] was is still there", vec.vec_data[1], &el[2]);

    return NULL;
}

static char * test_remove_all(void)
{
    struct data el[] = { { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 } };

    SVector_Init(&vec, 9, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    SVector_Remove(&vec, &(struct data){ 1 });
    SVector_Remove(&vec, &(struct data){ 5 });
    SVector_Remove(&vec, &(struct data){ 15 });
    SVector_Remove(&vec, &(struct data){ 800 });
    SVector_Remove(&vec, &(struct data){ 3 });
    SVector_Remove(&vec, &(struct data){ 300 });
    SVector_Remove(&vec, &(struct data){ 10 });
    SVector_Remove(&vec, &(struct data){ 20 });

    pu_assert_equal("the vector is empty", SVector_Size(&vec), 0);

    return NULL;
}

static char * test_pop(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    SVector_Init(&vec, 3, NULL);
    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    pu_assert_ptr_equal("Pops el[2]", SVector_Pop(&vec), &el[2]);
    pu_assert_ptr_equal("Pops el[1]", SVector_Pop(&vec), &el[1]);

    SVector_Insert(&vec, &el[0]);
    pu_assert_ptr_equal("Pops el[0]", SVector_Pop(&vec), &el[0]);
    pu_assert_ptr_equal("Pops el[0]", SVector_Pop(&vec), &el[0]);
    pu_assert_equal("Vector size is zeroed", SVector_Size(&vec), 0);

    return NULL;
}

static char * test_shift(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    SVector_Init(&vec, 3, NULL);
    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    pu_assert_ptr_equal("Shifts el[0]", SVector_Shift(&vec), &el[0]);
    pu_assert_ptr_equal("Shifts el[1]", SVector_Shift(&vec), &el[1]);
    pu_assert_ptr_equal("Shifts el[2]", SVector_Shift(&vec), &el[2]);
    pu_assert_equal("Vector size is zeroed", SVector_Size(&vec), 0);
    pu_assert_ptr_equal("Shifts NULL", SVector_Shift(&vec), NULL);

    return NULL;
}

static char * test_shift_reset(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 }, { 4 } };

    SVector_Init(&vec, num_elem(el), NULL);
    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    pu_assert_ptr_equal("Shifts el[0]", SVector_Shift(&vec), &el[0]);
    pu_assert_ptr_equal("Shifts el[1]", SVector_Shift(&vec), &el[1]);
    pu_assert_ptr_equal("Shifts el[2]", SVector_Shift(&vec), &el[2]);
    pu_assert_equal("shift index is changed", vec.vec_shift_index, 3);
    pu_assert_ptr_equal("Shifts el[3]", SVector_Shift(&vec), &el[3]);
    pu_assert_equal("shift index is reset", vec.vec_shift_index, 1);

    SVector_ShiftReset(&vec);
    pu_assert_equal("shift index is reset", vec.vec_shift_index, 0);

    return NULL;
}

static char * test_foreach(void)
{
    struct data el[] = { { 1 }, { 2 }, { 3 } };

    SVector_Init(&vec, 3, compar);
    for (size_t i = 0; i < num_elem(el); i++) {
        SVector_Insert(&vec, &el[i]);
    }

    size_t i = 0;
    struct data **d;
    /* cppcheck-suppress internalAstError */
    SVECTOR_FOREACH(d, &vec) {
        pu_assert_ptr_equal("el[0] is pointing to the correct item", *d, &el[i++]);
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
    pu_def_test(test_insertFast_many, PU_RUN);
    pu_def_test(test_insertFast_dedup, PU_RUN);
    pu_def_test(test_insert_no_compar, PU_RUN);
    pu_def_test(test_search, PU_RUN);
    pu_def_test(test_remove_one, PU_RUN);
    pu_def_test(test_remove_one_compound_literal, PU_RUN);
    pu_def_test(test_remove_last, PU_RUN);
    pu_def_test(test_remove_first, PU_RUN);
    pu_def_test(test_remove_middle, PU_RUN);
    pu_def_test(test_remove_all, PU_RUN);
    pu_def_test(test_pop, PU_RUN);
    pu_def_test(test_shift, PU_RUN);
    pu_def_test(test_shift_reset, PU_RUN);
    pu_def_test(test_foreach, PU_RUN);
}
