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
#include <dpcommon/output.h>
#include <dpcommon/vector.h>
#include <dptest.h>

#define ELEMENT_SIZE sizeof(int)


static void dump(DP_Output *output, DP_Vector *vector)
{
    size_t capacity = vector->capacity;
    size_t used = vector->used;
    DP_output_format(output, "capacity=%zu, used=%zu\n", capacity, used);

    int *elements = vector->elements;
    for (size_t i = 0; i < capacity; ++i) {
        if (i != 0) {
            DP_output_print(output, " ");
        }
        if (i < used) {
            DP_output_format(output, "[%d]", elements[i]);
        }
        else {
            DP_output_print(output, "[ ]");
        }
    }
    DP_output_print(output, "\n");
}

static void vec_init(DP_Output *output, DP_Vector *vector,
                     size_t initial_capacity)
{
    DP_output_format(output, "-- init(%zu)\n", initial_capacity);
    DP_vector_init(vector, initial_capacity, ELEMENT_SIZE);
    dump(output, vector);
}

static void vec_insert(DP_Output *output, DP_Vector *vector, size_t index,
                       int value)
{
    DP_output_format(output, "\n-- insert(%zu, %d)\n", index, value);
    int *element = DP_vector_insert(vector, ELEMENT_SIZE, index);
    *element = value;
    dump(output, vector);
}

static void vec_unshift(DP_Output *output, DP_Vector *vector, int value)
{
    DP_output_format(output, "\n-- unshift(%d)\n", value);
    int *element = DP_vector_unshift(vector, ELEMENT_SIZE);
    *element = value;
    dump(output, vector);
}

static void vec_push(DP_Output *output, DP_Vector *vector, int value)
{
    DP_output_format(output, "\n-- push(%d)\n", value);
    int *element = DP_vector_push(vector, ELEMENT_SIZE);
    *element = value;
    dump(output, vector);
}

static void vec_remove(DP_Output *output, DP_Vector *vector, size_t index)
{
    int *result = DP_vector_at(vector, ELEMENT_SIZE, index);
    DP_output_format(output, "\n-- remove(%zu) = %d\n", index, *result);
    DP_vector_remove(vector, ELEMENT_SIZE, index);
    dump(output, vector);
}

static void vec_shift(DP_Output *output, DP_Vector *vector)
{
    DP_output_print(output, "\n-- shift() = ");
    int *result = DP_vector_first(vector);
    if (result) {
        DP_output_format(output, "%d\n", *result);
    }
    else {
        DP_output_print(output, "NULL\n");
    }
    DP_vector_shift(vector, ELEMENT_SIZE);
    dump(output, vector);
}

static void vec_pop(DP_Output *output, DP_Vector *vector)
{
    DP_output_print(output, "\n-- pop() = ");
    int *result = DP_vector_last(vector, ELEMENT_SIZE);
    if (result) {
        DP_output_format(output, "%d\n", *result);
    }
    else {
        DP_output_print(output, "NULL\n");
    }
    DP_vector_pop(vector);
    dump(output, vector);
}


static void int_vector(TEST_PARAMS)
{
    DP_Output *output = DP_file_output_new_from_path("test/tmp/int_vector");
    DP_Vector vector;

    vec_init(output, &vector, 1);
    vec_push(output, &vector, 2);
    vec_push(output, &vector, 5);
    vec_unshift(output, &vector, 1);
    vec_insert(output, &vector, 2, 3);
    vec_insert(output, &vector, 3, 4);
    vec_remove(output, &vector, 2);
    vec_shift(output, &vector);
    vec_pop(output, &vector);
    vec_remove(output, &vector, 1);
    vec_remove(output, &vector, 0);
    vec_shift(output, &vector);
    vec_pop(output, &vector);

    DP_vector_dispose(&vector);
    DP_output_free(output);
    FILE_EQ_OK("test/tmp/int_vector", "test/data/int_vector",
               "int vector operation log matches expected");
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(int_vector);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
