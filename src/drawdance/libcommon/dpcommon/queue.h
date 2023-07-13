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
#ifndef DPCOMMON_QUEUE_H
#define DPCOMMON_QUEUE_H
#include <stdbool.h>
#include <stdlib.h>


#define DP_QUEUE_NULL \
    (DP_Queue)        \
    {                 \
        0, 0, 0, NULL \
    }

typedef struct DP_Queue {
    size_t capacity;
    size_t used;
    size_t head;
    void *elements;
} DP_Queue;

void DP_queue_init(DP_Queue *queue, size_t initial_capacity,
                   size_t element_size);

void DP_queue_dispose(DP_Queue *queue);

void DP_queue_clear(DP_Queue *queue, size_t element_size,
                    void (*dispose_element)(void *));

void *DP_queue_push(DP_Queue *queue, size_t element_size);

void *DP_queue_peek(DP_Queue *queue, size_t element_size);

void *DP_queue_peek_last(DP_Queue *queue, size_t element_size);

void *DP_queue_at(DP_Queue *queue, size_t element_size, size_t index);

void DP_queue_shift(DP_Queue *queue);

void DP_queue_pop(DP_Queue *queue);

void DP_queue_each(DP_Queue *queue, size_t element_size,
                   void (*fn)(void *element, void *user), void *user);

// Returns false if there's any elements in the queue that don't satisfy the
// predicate, otherwise returns true. Stops calling the predicate on the first
// false returned. An empty queue will return true.
bool DP_queue_all(DP_Queue *queue, size_t element_size,
                  bool (*predicate)(void *element, void *user), void *user);


#endif
