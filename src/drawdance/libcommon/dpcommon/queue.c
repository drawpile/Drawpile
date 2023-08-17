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
#include "queue.h"
#include "common.h"


void DP_queue_init(DP_Queue *queue, size_t initial_capacity,
                   size_t element_size)
{
    DP_ASSERT(queue);
    DP_ASSERT(initial_capacity > 0);
    DP_ASSERT(element_size > 0);
    void *elements = DP_malloc(initial_capacity * element_size);
    *queue = (DP_Queue){initial_capacity, 0, 0, elements};
}

void DP_queue_dispose(DP_Queue *queue)
{
    DP_ASSERT(queue);
    DP_free(queue->elements);
}

void DP_queue_clear(DP_Queue *queue, size_t element_size,
                    void (*dispose_element)(void *))
{
    DP_ASSERT(queue);
    DP_ASSERT(dispose_element);
    void *element;
    while ((element = DP_queue_peek(queue, element_size)) != NULL) {
        dispose_element(element);
        DP_queue_shift(queue);
    }
}

static void *element_at(DP_Queue *queue, size_t i, size_t element_size)
{
    DP_ASSERT(queue);
    DP_ASSERT(queue->elements);
    DP_ASSERT(i < queue->capacity);
    return (unsigned char *)queue->elements + i * element_size;
}

static size_t resize(DP_Queue *queue, size_t old_capacity, size_t element_size)
{
    DP_ASSERT(queue);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(queue->capacity == old_capacity);
    DP_ASSERT(queue->used == old_capacity);

    size_t new_capacity = old_capacity * 2;
    void *elements = DP_realloc(queue->elements, new_capacity * element_size);

    size_t old_head = queue->head;
    size_t length = old_capacity - old_head;
    size_t new_head = new_capacity - length;
    unsigned char *src = (unsigned char *)elements + old_head * element_size;
    unsigned char *dst = (unsigned char *)elements + new_head * element_size;
    memcpy(dst, src, length * element_size);

    queue->capacity = new_capacity;
    queue->head = new_head;
    queue->elements = elements;
    return new_capacity;
}

void *DP_queue_push(DP_Queue *queue, size_t element_size)
{
    DP_ASSERT(queue);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(queue->capacity > 0);
    DP_ASSERT(queue->used <= queue->capacity);
    size_t used = queue->used;
    size_t capacity = queue->capacity;
    if (used == capacity) {
        capacity = resize(queue, capacity, element_size);
    }
    size_t tail = (queue->head + used) % capacity;
    queue->used = used + 1;
    return element_at(queue, tail, element_size);
}

void *DP_queue_peek(DP_Queue *queue, size_t element_size)
{
    DP_ASSERT(queue);
    DP_ASSERT(element_size > 0);
    if (queue->used > 0) {
        return element_at(queue, queue->head, element_size);
    }
    else {
        return NULL;
    }
}

void *DP_queue_peek_last(DP_Queue *queue, size_t element_size)
{
    DP_ASSERT(queue);
    DP_ASSERT(element_size > 0);
    size_t used = queue->used;
    if (used > 0) {
        return element_at(queue, (queue->head + used - 1) % queue->capacity,
                          element_size);
    }
    else {
        return NULL;
    }
}

void *DP_queue_at(DP_Queue *queue, size_t element_size, size_t index)
{
    DP_ASSERT(queue);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(index < queue->used);
    return element_at(queue, (queue->head + index) % queue->capacity,
                      element_size);
}

void DP_queue_shift(DP_Queue *queue)
{
    if (queue->used > 0) {
        queue->head = (queue->head + 1) % queue->capacity;
        --queue->used;
    }
}

void DP_queue_pop(DP_Queue *queue)
{
    if (queue->used > 0) {
        --queue->used;
    }
}

void DP_queue_each(DP_Queue *queue, size_t element_size,
                   void (*fn)(void *element, void *user), void *user)
{
    DP_ASSERT(queue);
    DP_ASSERT(fn);
    size_t capacity = queue->capacity;
    size_t used = queue->used;
    size_t head = queue->head;
    for (size_t i = 0; i < used; ++i) {
        void *element = element_at(queue, (head + i) % capacity, element_size);
        fn(element, user);
    }
}

bool DP_queue_all(DP_Queue *queue, size_t element_size,
                  bool (*predicate)(void *element, void *user), void *user)
{
    DP_ASSERT(queue);
    DP_ASSERT(predicate);
    size_t capacity = queue->capacity;
    size_t used = queue->used;
    size_t head = queue->head;
    for (size_t i = 0; i < used; ++i) {
        void *element = element_at(queue, (head + i) % capacity, element_size);
        if (!predicate(element, user)) {
            return false;
        }
    }
    return true;
}

size_t DP_queue_search_index(DP_Queue *queue, size_t element_size,
                             bool (*predicate)(void *element, void *user),
                             void *user)
{
    DP_ASSERT(queue);
    DP_ASSERT(predicate);
    size_t capacity = queue->capacity;
    size_t used = queue->used;
    size_t head = queue->head;
    for (size_t i = 0; i < used; ++i) {
        void *element = element_at(queue, (head + i) % capacity, element_size);
        if (predicate(element, user)) {
            return i;
        }
    }
    return used;
}

size_t DP_queue_search_last_index(DP_Queue *queue, size_t element_size,
                                  bool (*predicate)(void *element, void *user),
                                  void *user)
{
    DP_ASSERT(queue);
    DP_ASSERT(predicate);
    size_t capacity = queue->capacity;
    size_t used = queue->used;
    size_t head = queue->head;
    for (size_t i = 0; i < used; ++i) {
        size_t j = used - i - 1;
        void *element = element_at(queue, (head + j) % capacity, element_size);
        if (predicate(element, user)) {
            return j;
        }
    }
    return used;
}
