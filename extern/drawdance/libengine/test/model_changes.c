/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpengine/model_changes.h>
#include <dptest_engine.h>


typedef struct TestModelEntry {
    int id;
    char value;
} TestModelEntry;

typedef struct TestModel {
    int count;
    TestModelEntry entries[];
} TestModel;

static void test_model_destroy(void *value)
{
    DP_free(value);
}

static TestModel *test_model_new(TestModelEntry *entries)
{
    int count = 0;
    while (entries[count].value != '\0') {
        ++count;
    }
    size_t entries_size = sizeof(*entries) * DP_int_to_size(count);
    TestModel *tm = DP_malloc(sizeof(*tm) + entries_size);
    tm->count = count;
    memcpy(tm->entries, entries, entries_size);
    return tm;
}

static int test_model_get_count(void *model)
{
    TestModel *tm = model;
    DP_ASSERT(tm);
    return tm->count;
}

static void *test_model_get_element(void *model, int i)
{
    TestModel *tm = model;
    DP_ASSERT(tm);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < tm->count);
    return &tm->entries[i];
}

static int test_model_get_index_by_id(void *model, int id)
{
    TestModel *tm = model;
    DP_ASSERT(tm);
    int count = tm->count;
    TestModelEntry *entries = tm->entries;
    for (int i = 0; i < count; ++i) {
        if (entries[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int test_model_get_id(void *element)
{
    TestModelEntry *entry = element;
    DP_ASSERT(entry);
    return entry->id;
}

static bool test_model_is_different(void *DP_RESTRICT prev_element,
                                    void *DP_RESTRICT next_element)
{
    TestModelEntry *prev_entry = prev_element;
    TestModelEntry *next_entry = next_element;
    DP_ASSERT(prev_entry);
    DP_ASSERT(next_entry);
    DP_ASSERT(prev_entry->id == next_entry->id);
    return prev_entry->value != next_entry->value;
}

static void test_model_added(int id, int index, void *element, void *user)
{
    TestModelEntry *entry = element;
    DP_ASSERT(entry);
    DP_output_format(user, "added %d '%c' at %d\n", id, entry->value, index);
}

static void test_model_removed(int id, void *user)
{
    DP_output_format(user, "removed %d\n", id);
}

static void test_model_moved(int id, int index, void *user)
{
    DP_output_format(user, "moved %d to %d\n", id, index);
}

static void test_model_changed(int id, void *element, void *user)
{
    TestModelEntry *entry = element;
    DP_ASSERT(entry);
    DP_output_format(user, "changed %d to '%c'\n", id, entry->value);
}

static const DP_ModelChangesMethods test_model_methods = {
    test_model_get_count, test_model_get_element,  test_model_get_index_by_id,
    test_model_get_id,    test_model_is_different, test_model_added,
    test_model_removed,   test_model_moved,        test_model_changed,
};


static void test_models(TEST_PARAMS, const char *name, TestModel *prev_model,
                        TestModel *next_model)
{
    char *actual_path = DP_format("test/tmp/model_changes_%s", name);
    DP_Output *output = DP_file_output_new_from_path(actual_path);
    FATAL(NOT_NULL_OK(output, "got output for %s", actual_path));
    DP_output_print(output, "begin diff\n");

    DP_ModelChanges *mc = DP_model_changes_new();
    DP_model_changes_diff(mc, prev_model, next_model, &test_model_methods,
                          output);
    DP_model_changes_free(mc);

    DP_output_print(output, "end diff\n");
    DP_output_free(output);

    char *expected_path = DP_format("test/data/model_changes_%s", name);
    FILE_EQ_OK(actual_path, expected_path, "model changes log matches");

    DP_free(expected_path);
    DP_free(actual_path);
}

static void null(TEST_PARAMS)
{
    test_models(TEST_ARGS, "null", NULL, NULL);
}

static void empty(TEST_PARAMS)
{
    TestModel *prev_model = test_model_new((TestModelEntry[]){{0, '\0'}});
    TestModel *next_model = test_model_new((TestModelEntry[]){{0, '\0'}});
    test_models(TEST_ARGS, "empty", prev_model, next_model);
    test_model_destroy(next_model);
    test_model_destroy(prev_model);
}

static void add_all(TEST_PARAMS)
{
    TestModel *next_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {2, 'b'}, {3, 'c'}, {0, '\0'}});
    test_models(TEST_ARGS, "add_all", NULL, next_model);
    test_model_destroy(next_model);
}

static void remove_all(TEST_PARAMS)
{
    TestModel *prev_model = test_model_new(
        (TestModelEntry[]){{4, 'd'}, {5, 'e'}, {6, 'f'}, {0, '\0'}});
    test_models(TEST_ARGS, "remove_all", prev_model, NULL);
    test_model_destroy(prev_model);
}

static void add_one(TEST_PARAMS)
{
    TestModel *prev_model =
        test_model_new((TestModelEntry[]){{1, 'a'}, {3, 'c'}, {0, '\0'}});
    TestModel *next_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {2, 'b'}, {3, 'c'}, {0, '\0'}});
    test_models(TEST_ARGS, "add_one", prev_model, next_model);
    test_model_destroy(next_model);
    test_model_destroy(prev_model);
}

static void remove_one(TEST_PARAMS)
{
    TestModel *prev_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {2, 'b'}, {3, 'c'}, {0, '\0'}});
    TestModel *next_model =
        test_model_new((TestModelEntry[]){{1, 'a'}, {3, 'c'}, {0, '\0'}});
    test_models(TEST_ARGS, "remove_one", prev_model, next_model);
    test_model_destroy(next_model);
    test_model_destroy(prev_model);
}

static void move_one(TEST_PARAMS)
{
    TestModel *prev_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {2, 'b'}, {3, 'c'}, {0, '\0'}});
    TestModel *next_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {3, 'c'}, {2, 'b'}, {0, '\0'}});
    test_models(TEST_ARGS, "move_one", prev_model, next_model);
    test_model_destroy(next_model);
    test_model_destroy(prev_model);
}

static void change_one(TEST_PARAMS)
{
    TestModel *prev_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {2, 'b'}, {3, 'c'}, {0, '\0'}});
    TestModel *next_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {2, 'b'}, {3, 'z'}, {0, '\0'}});
    test_models(TEST_ARGS, "change_one", prev_model, next_model);
    test_model_destroy(next_model);
    test_model_destroy(prev_model);
}

static void all_operations(TEST_PARAMS)
{
    TestModel *prev_model = test_model_new(
        (TestModelEntry[]){{1, 'a'}, {3, 'c'}, {5, 'e'}, {0, '\0'}});
    TestModel *next_model = test_model_new(
        (TestModelEntry[]){{3, 'x'}, {1, 'z'}, {4, 'd'}, {2, 'b'}, {0, '\0'}});
    test_models(TEST_ARGS, "all_operations", prev_model, next_model);
    test_model_destroy(next_model);
    test_model_destroy(prev_model);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(null);
    REGISTER_TEST(empty);
    REGISTER_TEST(add_all);
    REGISTER_TEST(remove_all);
    REGISTER_TEST(add_one);
    REGISTER_TEST(remove_one);
    REGISTER_TEST(move_one);
    REGISTER_TEST(change_one);
    REGISTER_TEST(all_operations);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
