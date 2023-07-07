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
#ifndef DPCOMMON_VECTOR_H
#define DPCOMMON_VECTOR_H
#include <stdbool.h>
#include <stdlib.h>


#define DP_VECTOR_NULL \
    (DP_Vector)        \
    {                  \
        0, 0, NULL     \
    }

#define DP_VECTOR_INIT_TYPE(VECTOR, TYPE, INITIAL_CAPACITY)         \
    do {                                                            \
        DP_vector_init((VECTOR), (INITIAL_CAPACITY), sizeof(TYPE)); \
    } while (0)

#define DP_VECTOR_CLEAR_TYPE(VECTOR, TYPE, DISPOSE)         \
    do {                                                    \
        DP_vector_clear((VECTOR), sizeof(TYPE), (DISPOSE)); \
    } while (0)

#define DP_VECTOR_CLEAR_DISPOSE_TYPE(VECTOR, TYPE, DISPOSE)         \
    do {                                                            \
        DP_vector_clear_dispose((VECTOR), sizeof(TYPE), (DISPOSE)); \
    } while (0)

#define DP_VECTOR_INSERT_TYPE(VECTOR, TYPE, INDEX, ELEMENT)                    \
    do {                                                                       \
        *((TYPE *)DP_vector_insert((VECTOR), sizeof(TYPE), (size_t)(INDEX))) = \
            (ELEMENT)                                                          \
    } while (0)

#define DP_VECTOR_UNSHIFT_TYPE(VECTOR, TYPE, ELEMENT)                     \
    do {                                                                  \
        (*(TYPE *)DP_vector_unshift((VECTOR), sizeof(TYPE))) = (ELEMENT); \
    } while (0)

#define DP_VECTOR_PUSH_TYPE(VECTOR, TYPE, ELEMENT)                     \
    do {                                                               \
        (*(TYPE *)DP_vector_push((VECTOR), sizeof(TYPE))) = (ELEMENT); \
    } while (0)

#define DP_VECTOR_FIRST_TYPE(VECTOR, TYPE) (*(TYPE *)DP_vector_first((VECTOR)))

#define DP_VECTOR_LAST_TYPE(VECTOR, TYPE) \
    (*(TYPE *)DP_vector_last((VECTOR), sizeof(TYPE)))

#define DP_VECTOR_AT_TYPE(VECTOR, TYPE, INDEX) \
    (*(TYPE *)DP_vector_at((VECTOR), sizeof(TYPE), (size_t)(INDEX)))

#define DP_VECTOR_REMOVE_TYPE(VECTOR, TYPE, INDEX)                 \
    do {                                                           \
        DP_vector_remove((VECTOR), sizeof(TYPE), (size_t)(INDEX)); \
    } while (0)

#define DP_VECTOR_SHIFT_TYPE(VECTOR, TYPE)       \
    do {                                         \
        DP_vector_shift((VECTOR), sizeof(TYPE)); \
    } while (0)

#define DP_VECTOR_SEARCH_INDEX_TYPE(VECTOR, TYPE, PREDICATE, USER) \
    DP_vector_search_index((VECTOR), sizeof(TYPE), (PREDICATE), (USER))

#define DP_VECTOR_SORT_TYPE(VECTOR, TYPE, COMPARE)         \
    do {                                                   \
        DP_vector_sort((VECTOR), sizeof(TYPE), (COMPARE)); \
    } while (0)


typedef struct DP_Vector {
    size_t capacity;
    size_t used;
    void *elements;
} DP_Vector;

void DP_vector_init(DP_Vector *vector, size_t initial_capacity,
                    size_t element_size);

void DP_vector_dispose(DP_Vector *vector);

void DP_vector_clear(DP_Vector *vector, size_t element_size,
                     void (*dispose_element)(void *));

void DP_vector_clear_dispose(DP_Vector *vector, size_t element_size,
                             void (*dispose_element)(void *));

void *DP_vector_insert(DP_Vector *vector, size_t element_size, size_t index);

void *DP_vector_unshift(DP_Vector *vector, size_t element_size);

void *DP_vector_push(DP_Vector *vector, size_t element_size);

void *DP_vector_at(DP_Vector *vector, size_t element_size, size_t index);

void *DP_vector_first(DP_Vector *vector);

void *DP_vector_last(DP_Vector *vector, size_t element_size);

void DP_vector_remove(DP_Vector *vector, size_t element_size, size_t index);

void DP_vector_shift(DP_Vector *vector, size_t element_size);

void DP_vector_pop(DP_Vector *vector);

int DP_vector_search_index(DP_Vector *vector, size_t element_size,
                           bool (*predicate)(void *element, void *user),
                           void *user);

void DP_vector_sort(DP_Vector *vector, size_t element_size,
                    int (*compare)(const void *, const void *));

#endif
