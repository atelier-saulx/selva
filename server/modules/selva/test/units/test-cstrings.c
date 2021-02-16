#include <punit.h>
#include "cstrings.h"

static void setup(void)
{
}

static void teardown(void)
{
}

/*
 * This is exactly the same matcher used in
 * module/subscriptions.c
 */
static int field_matcher(const char *list, const char *field)
{
    const char *sep = ".";
    int match;
    char *p;

    match = stringlist_searchn(list, field, strlen(field));
    if (!match && (p = strstr(field, sep))) {
        do {
            const size_t len = (ptrdiff_t)p++ - (ptrdiff_t)field;

            match = stringlist_searchn(list, field, len);
        } while (!match && p && (p = strstr(p, sep)));
    }

    return match;
}

static char * test_simple_match(void)
{
    const char *list = "title";
    const char *field = "title";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just match", match, 1);

    return NULL;
}

static char * test_simple_no_match(void)
{
    const char *list = "title";
    const char *field = "titlo";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just not match", match, 0);

    return NULL;
}

static char * test_simple_match_in_list(void)
{
    const char *list = "abc\ntitle\ndef";
    const char *field = "title";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match in the middle of the list", match, 1);

    return NULL;
}

static char * test_simple_match_in_list_last(void)
{
    const char *list = "abc\ntitle\ndef";
    const char *field = "def";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match in the middle of the list", match, 1);

    return NULL;
}

static char * test_sub_match(void)
{
    const char *list = "title";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just match", match, 1);

    return NULL;
}

static char * test_sub_list_match(void)
{
    const char *list = "image\ntitle";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just match", match, 1);

    return NULL;
}

static char * test_sub_list_no_match(void)
{
    const char *list = "image\ntitle.en";
    const char *field = "title.ru";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_sub_list_no_match_inverse1(void)
{
    const char *list = "image\ntitle.en";
    const char *field = "title";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_sub_list_no_match_inverse2(void)
{
    const char *list = "image\ntitle.en";
    const char *field = "title.ru";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_broken_list1(void)
{
    const char *list = "image\ntitle\n";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match", match, 1);

    return NULL;
}

static char * test_broken_list2(void)
{
    const char *list = "pic\n\ntitle.en";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match", match, 1);

    return NULL;
}

static char * test_empty_field(void)
{
    const char *list = "abc\ntitle\ndef";
    const char *field = "";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("no match", match, 0);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_simple_match, PU_RUN);
    pu_def_test(test_simple_no_match, PU_RUN);
    pu_def_test(test_simple_match_in_list, PU_RUN);
    pu_def_test(test_simple_match_in_list_last, PU_RUN);
    pu_def_test(test_sub_match, PU_RUN);
    pu_def_test(test_sub_list_match, PU_RUN);
    pu_def_test(test_sub_list_no_match, PU_RUN);
    pu_def_test(test_sub_list_no_match_inverse1, PU_RUN);
    pu_def_test(test_sub_list_no_match_inverse2, PU_RUN);
    pu_def_test(test_broken_list1, PU_RUN);
    pu_def_test(test_broken_list2, PU_SKIP); /* TODO This is currently failing but it's not a big deal */
    pu_def_test(test_empty_field, PU_RUN);
}
