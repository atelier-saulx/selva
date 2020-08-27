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
    struct data el[] = {
        { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 }, { 232 },
        { 223 }, { 130 }, { 132 }, { 133 }, { 134 }, { 135 }, { 136 }, { 137 },
        { 201 }, { 202 }, { 203 }, { 204 }, { 205 }, { 206 }, { 207 }, { 208 },
        { 301 }, { 302 }, { 303 }, { 304 }, { 305 }, { 306 }, { 307 }, { 308 },
        { 401 }, { 402 }, { 403 }, { 404 }, { 405 }, { 406 }, { 407 }, { 408 },
        { 501 }, { 502 }, { 503 }, { 504 }, { 505 }, { 506 }, { 507 }, { 508 },
        { 601 }, { 602 }, { 603 }, { 604 }, { 605 }, { 606 }, { 607 }, { 608 },
        { 0x00A7 }, { 0x8198 }, { 0x00A8 }, { 0x814E }, { 0x00B0 }, { 0x818B },
        { 0x00B4 }, { 0x814C }, { 0x00B6 }, { 0x81F7 }, { 0x00D7 }, { 0x817E },
        { 0x0391 }, { 0x839F }, { 0x0392 }, { 0x83A0 }, { 0x0393 }, { 0x83A1 },
        { 0x0395 }, { 0x83A3 }, { 0x0396 }, { 0x83A4 }, { 0x0397 }, { 0x83A5 },
        { 0x0399 }, { 0x83A7 }, { 0x039A }, { 0x83A8 }, { 0x039B }, { 0x83A9 },
        { 0x039D }, { 0x83AB }, { 0x039E }, { 0x83AC }, { 0x039F }, { 0x83AD },
        { 0x03A1 }, { 0x83AF }, { 0x03A3 }, { 0x83B0 }, { 0x03A4 }, { 0x83B1 },
        { 0x03A6 }, { 0x83B3 }, { 0x03A7 }, { 0x83B4 }, { 0x03A8 }, { 0x83B5 },
        { 0x03B1 }, { 0x83BF }, { 0x03B2 }, { 0x83C0 }, { 0x03B3 }, { 0x83C1 },
        { 0x03B5 }, { 0x83C3 }, { 0x03B6 }, { 0x83C4 }, { 0x03B7 }, { 0x83C5 },
        { 0x03B9 }, { 0x83C7 }, { 0x03BA }, { 0x83C8 }, { 0x03BB }, { 0x83C9 },
        { 0x03BD }, { 0x83CB }, { 0x03BE }, { 0x83CC }, { 0x03BF }, { 0x83CD },
        { 0x03C1 }, { 0x83CF }, { 0x03C3 }, { 0x83D0 }, { 0x03C4 }, { 0x83D1 },
        { 0x03C6 }, { 0x83D3 }, { 0x03C7 }, { 0x83D4 }, { 0x03C8 }, { 0x83D5 },
        { 0x0401 }, { 0x8446 }, { 0x0410 }, { 0x8440 }, { 0x0411 }, { 0x8441 },
        { 0x0413 }, { 0x8443 }, { 0x0414 }, { 0x8444 }, { 0x0415 }, { 0x8445 },
        { 0x0417 }, { 0x8448 }, { 0x0418 }, { 0x8449 }, { 0x0419 }, { 0x844A },
        { 0x041B }, { 0x844C }, { 0x041C }, { 0x844D }, { 0x041D }, { 0x844E },
        { 0x041F }, { 0x8450 }, { 0x0420 }, { 0x8451 }, { 0x0421 }, { 0x8452 },
        { 0x0423 }, { 0x8454 }, { 0x0424 }, { 0x8455 }, { 0x0425 }, { 0x8456 },
        { 0x0427 }, { 0x8458 }, { 0x0428 }, { 0x8459 }, { 0x0429 }, { 0x845A },
        { 0x042B }, { 0x845C }, { 0x042C }, { 0x845D }, { 0x042D }, { 0x845E },
        { 0x042F }, { 0x8460 }, { 0x0430 }, { 0x8470 }, { 0x0431 }, { 0x8471 },
        { 0x0433 }, { 0x8473 }, { 0x0434 }, { 0x8474 }, { 0x0435 }, { 0x8475 },
        { 0x0437 }, { 0x8478 }, { 0x0438 }, { 0x8479 }, { 0x0439 }, { 0x847A },
        { 0x043B }, { 0x847C }, { 0x043C }, { 0x847D }, { 0x043D }, { 0x847E },
        { 0x043F }, { 0x8481 }, { 0x0440 }, { 0x8482 }, { 0x0441 }, { 0x8483 },
        { 0x0443 }, { 0x8485 }, { 0x0444 }, { 0x8486 }, { 0x0445 }, { 0x8487 },
        { 0x0447 }, { 0x8489 }, { 0x0448 }, { 0x848A }, { 0x0449 }, { 0x848B },
        { 0x044B }, { 0x848D }, { 0x044C }, { 0x848E }, { 0x044D }, { 0x848F },
        { 0x044F }, { 0x8491 }, { 0x0451 }, { 0x8476 }, { 0x2010 }, { 0x815D },
        { 0x2018 }, { 0x8165 }, { 0x2019 }, { 0x8166 }, { 0x201C }, { 0x8167 },
        { 0x2020 }, { 0x81F5 }, { 0x2021 }, { 0x81F6 }, { 0x2025 }, { 0x8164 },
        { 0x2030 }, { 0x81F1 }, { 0x2032 }, { 0x818C }, { 0x2033 }, { 0x818D },
        { 0x2103 }, { 0x818E }, { 0x2116 }, { 0x8782 }, { 0x2121 }, { 0x8784 },
        { 0x2160 }, { 0x8754 }, { 0x2161 }, { 0x8755 }, { 0x2162 }, { 0x8756 },
        { 0x2164 }, { 0x8758 }, { 0x2165 }, { 0x8759 }, { 0x2166 }, { 0x875A },
        { 0x2168 }, { 0x875C }, { 0x2169 }, { 0x875D }, { 0x2170 }, { 0xFA40 },
        { 0x2172 }, { 0xFA42 }, { 0x2173 }, { 0xFA43 }, { 0x2174 }, { 0xFA44 },
        { 0x2176 }, { 0xFA46 }, { 0x2177 }, { 0xFA47 }, { 0x2178 }, { 0xFA48 },
        { 0x2190 }, { 0x81A9 }, { 0x2191 }, { 0x81AA }, { 0x2192 }, { 0x81A8 },
        { 0x21D2 }, { 0x81CB }, { 0x21D4 }, { 0x81CC }, { 0x2200 }, { 0x81CD },
        { 0x2203 }, { 0x81CE }, { 0x2207 }, { 0x81DE }, { 0x2208 }, { 0x81B8 },
        { 0x2211 }, { 0x8794 }, { 0x221A }, { 0x81E3 }, { 0x221D }, { 0x81E5 },
        { 0x221F }, { 0x8798 }, { 0x2220 }, { 0x81DA }, { 0x2225 }, { 0x8161 },
        { 0x2228 }, { 0x81C9 }, { 0x2229 }, { 0x81BF }, { 0x222A }, { 0x81BE },
    };

    SVector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        const void * r = SVector_InsertFast(&vec, &el[i]);
        pu_assert_ptr_equal("No return value", r, NULL);
    }

    pu_assert_equal("last is incremented", vec.vec_last, 333);

    struct data **data = ((struct data **)vec.vec_data);
    pu_assert_equal("el[0] was inserted correctly", data[0]->id, 1);
    pu_assert_equal("el[1] was inserted correctly", data[1]->id, 3);
    pu_assert_equal("el[2] was inserted correctly", data[2]->id, 5);
    pu_assert_equal("el[3] was inserted correctly", data[3]->id, 10);
    pu_assert_equal("el[4] was inserted correctly", data[4]->id, 15);
    pu_assert_equal("el[5] was inserted correctly", data[5]->id, 20);
    pu_assert_equal("el[6] was inserted correctly", data[6]->id, 130);
    pu_assert_equal("el[7] was inserted correctly", data[7]->id, 132);

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

static char * test_mixed_insertFast_and_Remove(void)
{
    struct data el[] = {
        { 1 }, { 5 }, { 15 }, { 800 }, { 3 }, { 300 }, { 10 }, { 20 }, { 232 },
        { 223 }, { 130 }, { 132 }, { 133 }, { 134 }, { 135 }, { 136 }, { 137 },
        { 201 }, { 202 }, { 203 }, { 204 }, { 205 }, { 206 }, { 207 }, { 208 },
        { 301 }, { 302 }, { 303 }, { 304 }, { 305 }, { 306 }, { 307 }, { 308 },
        { 401 }, { 402 }, { 403 }, { 404 }, { 405 }, { 406 }, { 407 }, { 408 },
        { 501 }, { 502 }, { 503 }, { 504 }, { 505 }, { 506 }, { 507 }, { 508 },
        { 601 }, { 602 }, { 603 }, { 604 }, { 605 }, { 606 }, { 607 }, { 608 },
        { 0x00A7 }, { 0x8198 }, { 0x00A8 }, { 0x814E }, { 0x00B0 }, { 0x818B },
        { 0x00B4 }, { 0x814C }, { 0x00B6 }, { 0x81F7 }, { 0x00D7 }, { 0x817E },
        { 0x0391 }, { 0x839F }, { 0x0392 }, { 0x83A0 }, { 0x0393 }, { 0x83A1 },
        { 0x0395 }, { 0x83A3 }, { 0x0396 }, { 0x83A4 }, { 0x0397 }, { 0x83A5 },
        { 0x0399 }, { 0x83A7 }, { 0x039A }, { 0x83A8 }, { 0x039B }, { 0x83A9 },
        { 0x039D }, { 0x83AB }, { 0x039E }, { 0x83AC }, { 0x039F }, { 0x83AD },
        { 0x03A1 }, { 0x83AF }, { 0x03A3 }, { 0x83B0 }, { 0x03A4 }, { 0x83B1 },
        { 0x03A6 }, { 0x83B3 }, { 0x03A7 }, { 0x83B4 }, { 0x03A8 }, { 0x83B5 },
        { 0x03B1 }, { 0x83BF }, { 0x03B2 }, { 0x83C0 }, { 0x03B3 }, { 0x83C1 },
        { 0x03B5 }, { 0x83C3 }, { 0x03B6 }, { 0x83C4 }, { 0x03B7 }, { 0x83C5 },
        { 0x03B9 }, { 0x83C7 }, { 0x03BA }, { 0x83C8 }, { 0x03BB }, { 0x83C9 },
        { 0x03BD }, { 0x83CB }, { 0x03BE }, { 0x83CC }, { 0x03BF }, { 0x83CD },
        { 0x03C1 }, { 0x83CF }, { 0x03C3 }, { 0x83D0 }, { 0x03C4 }, { 0x83D1 },
        { 0x03C6 }, { 0x83D3 }, { 0x03C7 }, { 0x83D4 }, { 0x03C8 }, { 0x83D5 },
        { 0x0401 }, { 0x8446 }, { 0x0410 }, { 0x8440 }, { 0x0411 }, { 0x8441 },
        { 0x0413 }, { 0x8443 }, { 0x0414 }, { 0x8444 }, { 0x0415 }, { 0x8445 },
        { 0x0417 }, { 0x8448 }, { 0x0418 }, { 0x8449 }, { 0x0419 }, { 0x844A },
        { 0x041B }, { 0x844C }, { 0x041C }, { 0x844D }, { 0x041D }, { 0x844E },
        { 0x041F }, { 0x8450 }, { 0x0420 }, { 0x8451 }, { 0x0421 }, { 0x8452 },
        { 0x0423 }, { 0x8454 }, { 0x0424 }, { 0x8455 }, { 0x0425 }, { 0x8456 },
        { 0x0427 }, { 0x8458 }, { 0x0428 }, { 0x8459 }, { 0x0429 }, { 0x845A },
        { 0x042B }, { 0x845C }, { 0x042C }, { 0x845D }, { 0x042D }, { 0x845E },
        { 0x042F }, { 0x8460 }, { 0x0430 }, { 0x8470 }, { 0x0431 }, { 0x8471 },
        { 0x0433 }, { 0x8473 }, { 0x0434 }, { 0x8474 }, { 0x0435 }, { 0x8475 },
        { 0x0437 }, { 0x8478 }, { 0x0438 }, { 0x8479 }, { 0x0439 }, { 0x847A },
        { 0x043B }, { 0x847C }, { 0x043C }, { 0x847D }, { 0x043D }, { 0x847E },
        { 0x043F }, { 0x8481 }, { 0x0440 }, { 0x8482 }, { 0x0441 }, { 0x8483 },
        { 0x0443 }, { 0x8485 }, { 0x0444 }, { 0x8486 }, { 0x0445 }, { 0x8487 },
        { 0x0447 }, { 0x8489 }, { 0x0448 }, { 0x848A }, { 0x0449 }, { 0x848B },
        { 0x044B }, { 0x848D }, { 0x044C }, { 0x848E }, { 0x044D }, { 0x848F },
        { 0x044F }, { 0x8491 }, { 0x0451 }, { 0x8476 }, { 0x2010 }, { 0x815D },
        { 0x2018 }, { 0x8165 }, { 0x2019 }, { 0x8166 }, { 0x201C }, { 0x8167 },
        { 0x2020 }, { 0x81F5 }, { 0x2021 }, { 0x81F6 }, { 0x2025 }, { 0x8164 },
        { 0x2030 }, { 0x81F1 }, { 0x2032 }, { 0x818C }, { 0x2033 }, { 0x818D },
        { 0x2103 }, { 0x818E }, { 0x2116 }, { 0x8782 }, { 0x2121 }, { 0x8784 },
        { 0x2160 }, { 0x8754 }, { 0x2161 }, { 0x8755 }, { 0x2162 }, { 0x8756 },
        { 0x2164 }, { 0x8758 }, { 0x2165 }, { 0x8759 }, { 0x2166 }, { 0x875A },
        { 0x2168 }, { 0x875C }, { 0x2169 }, { 0x875D }, { 0x2170 }, { 0xFA40 },
        { 0x2172 }, { 0xFA42 }, { 0x2173 }, { 0xFA43 }, { 0x2174 }, { 0xFA44 },
        { 0x2176 }, { 0xFA46 }, { 0x2177 }, { 0xFA47 }, { 0x2178 }, { 0xFA48 },
        { 0x2190 }, { 0x81A9 }, { 0x2191 }, { 0x81AA }, { 0x2192 }, { 0x81A8 },
        { 0x21D2 }, { 0x81CB }, { 0x21D4 }, { 0x81CC }, { 0x2200 }, { 0x81CD },
        { 0x2203 }, { 0x81CE }, { 0x2207 }, { 0x81DE }, { 0x2208 }, { 0x81B8 },
        { 0x2211 }, { 0x8794 }, { 0x221A }, { 0x81E3 }, { 0x221D }, { 0x81E5 },
        { 0x221F }, { 0x8798 }, { 0x2220 }, { 0x81DA }, { 0x2225 }, { 0x8161 },
        { 0x2228 }, { 0x81C9 }, { 0x2229 }, { 0x81BF }, { 0x222A }, { 0x81BE },
    };

    SVector_Init(&vec, 5, compar);

    for (size_t i = 0; i < num_elem(el); i++) {
        const void * r;

        r = SVector_InsertFast(&vec, &el[i]);
        pu_assert_ptr_equal("no return value", r, NULL);

        if (i > 0 &&i % 3 == 0) {
            r = SVector_Remove(&vec, &el[i - 1]);
            pu_assert_ptr_equal("returned the correct el", r, &el[i - 1]);
        }
    }

    pu_assert_equal("final size is correct", SVector_Size(&vec), 223);

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
    pu_def_test(test_mixed_insertFast_and_Remove, PU_RUN);
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
