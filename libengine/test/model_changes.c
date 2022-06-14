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
#include <dpengine_test.h>


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

static TestModel *test_model_new(void **state, TestModelEntry *entries)
{
    int count = 0;
    while (entries[count].value != '\0') {
        ++count;
    }
    size_t entries_size = sizeof(*entries) * DP_int_to_size(count);
    TestModel *tm = DP_malloc(sizeof(*tm) + entries_size);
    destructor_push(state, tm, test_model_destroy);
    tm->count = count;
    memcpy(tm->entries, entries, entries_size);
    return tm;
}

static int test_model_get_count(void *model)
{
    TestModel *tm = model;
    assert_non_null(tm);
    return tm->count;
}

static void *test_model_get_element(void *model, int i)
{
    TestModel *tm = model;
    assert_non_null(tm);
    assert_true(i >= 0);
    assert_true(i < tm->count);
    return &tm->entries[i];
}

static int test_model_get_index_by_id(void *model, int id)
{
    TestModel *tm = model;
    assert_non_null(tm);
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
    assert_non_null(entry);
    return entry->id;
}

static bool test_model_is_different(void *DP_RESTRICT prev_element,
                                    void *DP_RESTRICT next_element)
{
    TestModelEntry *prev_entry = prev_element;
    TestModelEntry *next_entry = next_element;
    assert_non_null(prev_entry);
    assert_non_null(next_entry);
    assert_int_equal(prev_entry->id, next_entry->id);
    return prev_entry->value != next_entry->value;
}

static void test_model_added(int id, int index, void *element, void *user)
{
    TestModelEntry *entry = element;
    assert_non_null(entry);
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
    assert_non_null(entry);
    DP_output_format(user, "changed %d to '%c'\n", id, entry->value);
}

static const DP_ModelChangesMethods test_model_methods = {
    test_model_get_count, test_model_get_element,  test_model_get_index_by_id,
    test_model_get_id,    test_model_is_different, test_model_added,
    test_model_removed,   test_model_moved,        test_model_changed,
};


static void test_models(void **state, const char *name, TestModel *prev_model,
                        TestModel *next_model)
{
    DP_ModelChanges *mc = DP_model_changes_new();
    push_model_changes(state, mc);

    char *actual_path = DP_format("test/tmp/model_changes_%s", name);
    destructor_push(state, actual_path, DP_free);
    DP_Output *output = DP_file_output_new_from_path(actual_path);
    push_output(state, output);

    DP_output_print(output, "begin diff\n");
    DP_model_changes_diff(mc, prev_model, next_model, &test_model_methods,
                          output);
    DP_output_print(output, "end diff\n");

    destructor_run(state, output);
    char *expected_path = DP_format("test/data/model_changes_%s", name);
    destructor_push(state, expected_path, DP_free);
    assert_files_equal(actual_path, expected_path);
}

static void null(void **state)
{
    test_models(state, "null", NULL, NULL);
}

static void empty(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {0, '\0'},
                                                  });
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {0, '\0'},
                                                  });
    test_models(state, "empty", prev_model, next_model);
}

static void add_all(void **state)
{
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {2, 'b'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "add_all", NULL, next_model);
}

static void remove_all(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {4, 'd'},
                                                      {5, 'e'},
                                                      {6, 'f'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "remove_all", prev_model, NULL);
}

static void add_one(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {2, 'b'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "add_one", prev_model, next_model);
}

static void remove_one(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {2, 'b'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "remove_one", prev_model, next_model);
}

static void move_one(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {2, 'b'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {3, 'c'},
                                                      {2, 'b'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "move_one", prev_model, next_model);
}

static void change_one(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {2, 'b'},
                                                      {3, 'c'},
                                                      {0, '\0'},
                                                  });
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {2, 'b'},
                                                      {3, 'z'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "change_one", prev_model, next_model);
}

static void all_operations(void **state)
{
    TestModel *prev_model = test_model_new(state, (TestModelEntry[]){
                                                      {1, 'a'},
                                                      {3, 'c'},
                                                      {5, 'e'},
                                                      {0, '\0'},
                                                  });
    TestModel *next_model = test_model_new(state, (TestModelEntry[]){
                                                      {3, 'x'},
                                                      {1, 'z'},
                                                      {4, 'd'},
                                                      {2, 'b'},
                                                      {0, '\0'},
                                                  });
    test_models(state, "all_operations", prev_model, next_model);
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        dp_unit_test(null),           dp_unit_test(empty),
        dp_unit_test(add_all),        dp_unit_test(remove_all),
        dp_unit_test(add_one),        dp_unit_test(remove_one),
        dp_unit_test(move_one),       dp_unit_test(change_one),
        dp_unit_test(all_operations),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
