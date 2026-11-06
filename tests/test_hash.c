/* Quoted path so a syntax check without -I still finds the public header. */
#include "../include/hash.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int tests_run = 0;
static int tests_failed = 0;
static const char *current_test = NULL;

static void expect(int condition, const char *file, int line, const char *expr)
{
    if (!condition) {
        printf("FAIL %s %s:%d: %s\n",
               current_test != NULL ? current_test : "?",
               file, line, expr);
        fflush(stdout);
        failures++;
    }
}

#define EXPECT(cond) expect((cond), __FILE__, __LINE__, #cond)

static int semver_major_minor(const char *version)
{
    int parts = 0;

    if (version == NULL) {
        return 0;
    }

    while (parts < 2) {
        if (*version < '0' || *version > '9') {
            return 0;
        }
        while (*version >= '0' && *version <= '9') {
            version++;
        }
        parts++;
        if (parts == 2) {
            return 1;
        }
        if (*version != '.') {
            return 0;
        }
        version++;
    }
    return 0;
}

struct walk {
    int visits;
    int stop;
    const char *seen;
};

static int walk_fn(const char *key, void *extra)
{
    struct walk *walk = extra;

    walk->visits++;
    walk->seen = key;
    if (walk->stop) {
        return HASH_INVAL;
    }
    return HASH_OK;
}

static void test_version_string_is_semver(void)
{
    EXPECT(semver_major_minor(HASH_VERSION_STRING));
}

static void test_create_and_destroy(void)
{
    hash *table = NULL;
    hash *kept = NULL;

    EXPECT(hash_create(NULL) == HASH_INVAL);
    EXPECT(hash_create(&kept) == HASH_OK);
    EXPECT(hash_count(kept) == 0);
    EXPECT(hash_count(NULL) == 0);
    EXPECT(hash_destroy(&kept) == HASH_OK);
    EXPECT(kept == NULL);
    EXPECT(hash_destroy(&kept) == HASH_OK);
    EXPECT(hash_destroy(NULL) == HASH_INVAL);
    EXPECT(hash_delete(NULL, "key") == HASH_INVAL);
    EXPECT(hash_delete(table, NULL) == HASH_INVAL);
    EXPECT(hash_each(NULL, walk_fn, NULL) == HASH_INVAL);
}

static void test_strings_are_case_insensitive(void)
{
    hash *table = NULL;
    const char *value = "keep";
    struct walk walk;

    EXPECT(hash_create(&table) == HASH_OK);
    EXPECT(hash_put_string(table, NULL, "x") == HASH_INVAL);
    EXPECT(hash_put_string(table, "k", NULL) == HASH_INVAL);
    EXPECT(hash_put_string(NULL, "k", "x") == HASH_INVAL);
    EXPECT(hash_put_string(table, "", "empty-key") == HASH_OK);
    EXPECT(hash_put_string(table, "e", "") == HASH_OK);
    EXPECT(hash_put_string(table, "Foo", "one") == HASH_OK);
    EXPECT(hash_put_string(table, "FOO", NULL) == HASH_INVAL);
    EXPECT(hash_get_string(table, "foo", &value) == HASH_OK);
    EXPECT(strcmp(value, "one") == 0);
    EXPECT(hash_put_string(table, "foo", "two") == HASH_OK);
    EXPECT(hash_get_string(table, "Foo", &value) == HASH_OK);
    EXPECT(strcmp(value, "two") == 0);
    EXPECT(hash_get_string(table, "", &value) == HASH_OK);
    EXPECT(strcmp(value, "empty-key") == 0);
    EXPECT(hash_get_string(table, "e", &value) == HASH_OK);
    EXPECT(strcmp(value, "") == 0);
    EXPECT(hash_count(table) == 3);

    value = "keep";
    EXPECT(hash_get_string(table, "missing", &value) == HASH_INVAL);
    EXPECT(strcmp(value, "keep") == 0);
    EXPECT(hash_get_string(table, "foo", NULL) == HASH_INVAL);

    memset(&walk, 0, sizeof walk);
    EXPECT(hash_each(table, walk_fn, &walk) == HASH_OK);
    EXPECT(walk.visits == 3);
    EXPECT(hash_put_int(table, "n", 7) == HASH_OK);
    value = "keep";
    EXPECT(hash_get_string(table, "n", &value) == HASH_INVAL);
    EXPECT(strcmp(value, "keep") == 0);
    EXPECT(hash_destroy(&table) == HASH_OK);
}

static void test_int_and_time_keep_their_bits(void)
{
    hash *table = NULL;
    int number = 42;
    time_t when = (time_t)99;

    EXPECT(hash_create(&table) == HASH_OK);
    EXPECT(hash_put_int(table, "zero", 0) == HASH_OK);
    EXPECT(hash_put_int(table, "min", INT_MIN) == HASH_OK);
    EXPECT(hash_put_int(table, "max", INT_MAX) == HASH_OK);
    EXPECT(hash_get_int(table, "ZERO", &number) == HASH_OK);
    EXPECT(number == 0);
    EXPECT(hash_get_int(table, "min", &number) == HASH_OK);
    EXPECT(number == INT_MIN);
    EXPECT(hash_get_int(table, "max", &number) == HASH_OK);
    EXPECT(number == INT_MAX);
    number = 42;
    EXPECT(hash_get_int(table, "missing", &number) == HASH_INVAL);
    EXPECT(number == 42);
    EXPECT(hash_get_int(table, "zero", NULL) == HASH_INVAL);

    EXPECT(hash_put_time(table, "epoch", (time_t)0) == HASH_OK);
    EXPECT(hash_put_time(table, "later", (time_t)1234567890) == HASH_OK);
    EXPECT(hash_put_time(table, "before", (time_t)-1) == HASH_OK);
    EXPECT(hash_get_time(table, "epoch", &when) == HASH_OK);
    EXPECT(when == (time_t)0);
    EXPECT(hash_get_time(table, "later", &when) == HASH_OK);
    EXPECT(when == (time_t)1234567890);
    EXPECT(hash_get_time(table, "before", &when) == HASH_OK);
    EXPECT(when == (time_t)-1);
    when = (time_t)99;
    EXPECT(hash_get_time(table, "missing", &when) == HASH_INVAL);
    EXPECT(when == (time_t)99);
    EXPECT(hash_get_time(table, "epoch", NULL) == HASH_INVAL);
    number = 42;
    EXPECT(hash_get_int(table, "epoch", &number) == HASH_INVAL);
    EXPECT(number == 42);
    EXPECT(hash_destroy(&table) == HASH_OK);
}

static void test_pointer_null_is_a_value(void)
{
    hash *table = NULL;
    int live = 17;
    void *value = &live;

    EXPECT(hash_create(&table) == HASH_OK);
    EXPECT(hash_put_ptr(NULL, "p", &live) == HASH_INVAL);
    EXPECT(hash_put_ptr(table, NULL, &live) == HASH_INVAL);
    EXPECT(hash_put_ptr(table, "p", &live) == HASH_OK);
    EXPECT(hash_get_ptr(table, "p", &value) == HASH_OK);
    EXPECT(value == &live);
    EXPECT(hash_put_ptr(table, "p", NULL) == HASH_OK);
    EXPECT(hash_get_ptr(table, "P", &value) == HASH_OK);
    EXPECT(value == NULL);
    EXPECT(live == 17);
    value = &live;
    EXPECT(hash_get_ptr(table, "missing", &value) == HASH_INVAL);
    EXPECT(value == &live);
    EXPECT(hash_delete(table, "p") == HASH_OK);
    EXPECT(live == 17);
    EXPECT(hash_get_ptr(table, "p", &value) == HASH_INVAL);
    EXPECT(value == &live);
    EXPECT(hash_destroy(&table) == HASH_OK);
}

static void test_nested_hash_is_owned(void)
{
    hash *parent = NULL;
    hash *other = NULL;
    hash *child = NULL;
    hash *replacement = NULL;
    hash *found = NULL;
    const char *value = NULL;

    EXPECT(hash_create(&parent) == HASH_OK);
    EXPECT(hash_create(&other) == HASH_OK);
    EXPECT(hash_create(&child) == HASH_OK);
    EXPECT(hash_create(&replacement) == HASH_OK);
    EXPECT(hash_put_string(child, "inner", "yes") == HASH_OK);
    EXPECT(hash_put_hash(parent, "c", NULL) == HASH_INVAL);
    EXPECT(hash_put_hash(parent, "c", parent) == HASH_INVAL);
    EXPECT(hash_count(parent) == 0);
    EXPECT(hash_put_hash(parent, "c", child) == HASH_OK);
    EXPECT(hash_put_hash(parent, "C", child) == HASH_OK);
    EXPECT(hash_put_hash(other, "c", child) == HASH_INVAL);
    EXPECT(hash_put_hash(child, "up", parent) == HASH_INVAL);
    EXPECT(hash_destroy(&child) == HASH_INVAL);
    EXPECT(child != NULL);
    EXPECT(hash_get_hash(parent, "c", &found) == HASH_OK);
    EXPECT(found == child);
    EXPECT(hash_get_string(child, "inner", &value) == HASH_OK);
    EXPECT(strcmp(value, "yes") == 0);

    EXPECT(hash_put_hash(parent, "c", replacement) == HASH_OK);
    found = NULL;
    EXPECT(hash_get_hash(parent, "c", &found) == HASH_OK);
    EXPECT(found == replacement);
    EXPECT(hash_destroy(&replacement) == HASH_INVAL);
    EXPECT(hash_destroy(&parent) == HASH_OK);
    EXPECT(hash_destroy(&other) == HASH_OK);
}

static void test_delete_count_and_grow(void)
{
    hash *table = NULL;
    hash *other = NULL;
    const char *value = "keep";
    char key[16];
    char body[16];
    int i;

    EXPECT(hash_create(&table) == HASH_OK);
    EXPECT(hash_create(&other) == HASH_OK);
    for (i = 0; i < 40; i++) {
        snprintf(key, sizeof key, "k%02d", i);
        snprintf(body, sizeof body, "v%02d", i);
        EXPECT(hash_put_string(table, key, body) == HASH_OK);
    }
    EXPECT(hash_count(table) == 40);
    EXPECT(hash_count(other) == 0);
    EXPECT(hash_get_string(table, "k00", &value) == HASH_OK);
    EXPECT(strcmp(value, "v00") == 0);
    EXPECT(hash_get_string(table, "k39", &value) == HASH_OK);
    EXPECT(strcmp(value, "v39") == 0);
    EXPECT(hash_delete(table, "missing") == HASH_OK);
    EXPECT(hash_count(table) == 40);
    EXPECT(hash_delete(table, "K00") == HASH_OK);
    EXPECT(hash_count(table) == 39);
    value = "keep";
    EXPECT(hash_get_string(table, "k00", &value) == HASH_INVAL);
    EXPECT(strcmp(value, "keep") == 0);
    EXPECT(hash_get_string(table, "k39", &value) == HASH_OK);
    EXPECT(strcmp(value, "v39") == 0);
    EXPECT(hash_put_string(other, "k39", "elsewhere") == HASH_OK);
    EXPECT(hash_get_string(table, "k39", &value) == HASH_OK);
    EXPECT(strcmp(value, "v39") == 0);
    EXPECT(hash_destroy(&table) == HASH_OK);
    EXPECT(hash_destroy(&other) == HASH_OK);
}

static void test_each_walks_keys(void)
{
    hash *table = NULL;
    struct walk walk;

    EXPECT(hash_create(&table) == HASH_OK);
    memset(&walk, 0, sizeof walk);
    EXPECT(hash_each(table, NULL, &walk) == HASH_INVAL);
    EXPECT(hash_each(table, walk_fn, &walk) == HASH_OK);
    EXPECT(walk.visits == 0);

    EXPECT(hash_put_string(table, "Foo", "one") == HASH_OK);
    EXPECT(hash_put_string(table, "bar", "two") == HASH_OK);
    memset(&walk, 0, sizeof walk);
    walk.stop = 1;
    EXPECT(hash_each(table, walk_fn, &walk) == HASH_INVAL);
    EXPECT(walk.visits == 1);

    memset(&walk, 0, sizeof walk);
    EXPECT(hash_delete(table, "bar") == HASH_OK);
    EXPECT(hash_each(table, walk_fn, &walk) == HASH_OK);
    EXPECT(walk.visits == 1);
    EXPECT(strcmp(walk.seen, "Foo") == 0);
    EXPECT(hash_destroy(&table) == HASH_OK);
}

static void run_test(const char *name, void (*fn)(void))
{
    int before = failures;

    current_test = name;
    fn();
    tests_run++;
    if (failures == before) {
        printf("tests/test_hash.c::%s PASSED\n", name);
    } else {
        tests_failed++;
        printf("tests/test_hash.c::%s FAILED\n", name);
    }
    fflush(stdout);
}

int main(void)
{
    int passed;

    run_test("test_version_string_is_semver", test_version_string_is_semver);
    run_test("test_create_and_destroy", test_create_and_destroy);
    run_test("test_strings_are_case_insensitive", test_strings_are_case_insensitive);
    run_test("test_int_and_time_keep_their_bits", test_int_and_time_keep_their_bits);
    run_test("test_pointer_null_is_a_value", test_pointer_null_is_a_value);
    run_test("test_nested_hash_is_owned", test_nested_hash_is_owned);
    run_test("test_delete_count_and_grow", test_delete_count_and_grow);
    run_test("test_each_walks_keys", test_each_walks_keys);

    passed = tests_run - tests_failed;
    if (tests_failed != 0) {
        printf("%d failed, %d passed, %d assertion(s) failed\n",
               tests_failed, passed, failures);
        return 1;
    }
    printf("%d passed\n", passed);
    return 0;
}
