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
#include "vector.h"
#include "common.h"
#include "conversions.h"

void DP_vector_init(DP_Vector *vector, size_t initial_capacity,
                    size_t element_size)
{
    DP_ASSERT(vector);
    DP_ASSERT(initial_capacity > 0);
    DP_ASSERT(element_size > 0);
    void *elements = DP_malloc(initial_capacity * element_size);
    *vector = (DP_Vector){initial_capacity, 0, elements};
}

void DP_vector_dispose(DP_Vector *vector)
{
    DP_ASSERT(vector);
    DP_free(vector->elements);
}

void DP_vector_clear(DP_Vector *vector, size_t element_size,
                     void (*dispose_element)(void *))
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(dispose_element);
    size_t used = vector->used;
    for (size_t i = 0; i < used; ++i) {
        void *element = DP_vector_at(vector, element_size, i);
        dispose_element(element);
    }
    vector->used = 0;
}

void DP_vector_clear_dispose(DP_Vector *vector, size_t element_size,
                             void (*dispose_element)(void *))
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(dispose_element);
    DP_vector_clear(vector, element_size, dispose_element);
    DP_vector_dispose(vector);
}

static void resize(DP_Vector *vector, size_t old_capacity, size_t element_size)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(vector->capacity > 0);
    DP_ASSERT(vector->used <= vector->capacity);
    size_t new_capacity = old_capacity * 2;
    size_t new_size = new_capacity * element_size;
    vector->elements = DP_realloc(vector->elements, new_size);
    vector->capacity = new_capacity;
}

void *DP_vector_insert(DP_Vector *vector, size_t element_size, size_t index)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(index <= vector->used);
    DP_ASSERT(vector->capacity > 0);
    DP_ASSERT(vector->used <= vector->capacity);
    if (index == 0) {
        return DP_vector_unshift(vector, element_size);
    }
    else {
        size_t used = vector->used;
        if (index == used) {
            return DP_vector_push(vector, element_size);
        }
        else {
            size_t capacity = vector->capacity;
            if (used == capacity) {
                resize(vector, capacity, element_size);
            }
            void *element = DP_vector_at(vector, element_size, index);
            memmove((unsigned char *)element + element_size, element,
                    element_size * (used - index));
            vector->used = used + 1;
            return element;
        }
    }
}

void *DP_vector_unshift(DP_Vector *vector, size_t element_size)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(vector->capacity > 0);
    DP_ASSERT(vector->used <= vector->capacity);
    size_t used = vector->used;
    size_t capacity = vector->capacity;
    if (used == capacity) {
        resize(vector, capacity, element_size);
    }
    memmove((unsigned char *)vector->elements + element_size, vector->elements,
            element_size * used);
    vector->used = used + 1;
    return vector->elements;
}

void *DP_vector_push(DP_Vector *vector, size_t element_size)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(vector->capacity > 0);
    DP_ASSERT(vector->used <= vector->capacity);
    size_t used = vector->used;
    size_t capacity = vector->capacity;
    if (used == capacity) {
        resize(vector, capacity, element_size);
    }
    vector->used = used + 1;
    return DP_vector_at(vector, element_size, used);
}

void *DP_vector_at(DP_Vector *vector, size_t element_size, size_t index)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(index < vector->used);
    return (unsigned char *)vector->elements + index * element_size;
}

void *DP_vector_first(DP_Vector *vector)
{
    DP_ASSERT(vector);
    return vector->used > 0 ? vector->elements : NULL;
}

void *DP_vector_last(DP_Vector *vector, size_t element_size)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    size_t used = vector->used;
    return used > 0 ? DP_vector_at(vector, element_size, used - 1) : NULL;
}

void DP_vector_remove(DP_Vector *vector, size_t element_size, size_t index)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(index < vector->used);
    if (index == 0) {
        DP_vector_shift(vector, element_size);
    }
    else {
        size_t used = vector->used;
        if (index == used - 1) {
            DP_vector_pop(vector);
        }
        else {
            void *element = DP_vector_at(vector, element_size, index);
            memmove(element, (unsigned char *)element + element_size,
                    element_size * (used - index));
            vector->used = used - 1;
        }
    }
}

void DP_vector_shift(DP_Vector *vector, size_t element_size)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    size_t used = vector->used;
    if (used > 0) {
        size_t new_used = used - 1;
        memmove(vector->elements,
                (unsigned char *)vector->elements + element_size,
                element_size * new_used);
        vector->used = new_used;
    }
}

void DP_vector_pop(DP_Vector *vector)
{
    DP_ASSERT(vector);
    size_t used = vector->used;
    if (used > 0) {
        vector->used = used - 1;
    }
}

int DP_vector_search_index(DP_Vector *vector, size_t element_size,
                           bool (*predicate)(void *element, void *user),
                           void *user)
{
    DP_ASSERT(vector);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(predicate);
    size_t used = vector->used;
    for (size_t i = 0; i < used; ++i) {
        if (predicate(DP_vector_at(vector, element_size, i), user)) {
            return DP_size_to_int(i);
        }
    }
    return -1;
}
