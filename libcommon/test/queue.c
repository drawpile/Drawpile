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
#include <dpcommon/queue.h>
#include <dptest.h>

#define ELEMENT_SIZE sizeof(int)


static void dump(DP_Output *output, DP_Queue *queue)
{
    size_t capacity = queue->capacity;
    size_t head = queue->head;
    size_t used = queue->used;
    size_t tail = (head + used) % capacity;
    DP_output_format(output, "capacity=%zu, used=%zu, head=%zu, tail=%zu\n",
                     capacity, used, head, tail);

    int *elements = queue->elements;
    for (size_t i = 0; i < capacity; ++i) {
        if (i != 0) {
            DP_output_print(output, " ");
        }
        if (used == capacity
            || (head <= tail ? i >= head && i < tail : i >= head || i < tail)) {
            DP_output_format(output, "[%d]", elements[i]);
        }
        else {
            DP_output_print(output, "[ ]");
        }
    }
    DP_output_print(output, "\n");
}

static void init(DP_Output *output, DP_Queue *queue, size_t initial_capacity)
{
    DP_output_format(output, "-- init(%zu)\n", initial_capacity);
    DP_queue_init(queue, initial_capacity, ELEMENT_SIZE);
    dump(output, queue);
}

static void push(DP_Output *output, DP_Queue *queue, int value)
{
    DP_output_format(output, "\n-- push(%d)\n", value);
    int *element = DP_queue_push(queue, ELEMENT_SIZE);
    *element = value;
    dump(output, queue);
}

static void shift(DP_Output *output, DP_Queue *queue)
{
    DP_output_print(output, "\n-- shift() = ");
    int *result = DP_queue_peek(queue, ELEMENT_SIZE);
    if (result) {
        DP_output_format(output, "%d\n", *result);
    }
    else {
        DP_output_print(output, "NULL\n");
    }
    DP_queue_shift(queue);
    dump(output, queue);
}


static void int_queue(TEST_PARAMS)
{
    DP_Output *output = DP_file_output_new_from_path("test/tmp/int_queue");
    DP_Queue queue;

    init(output, &queue, 2);
    push(output, &queue, 1);
    push(output, &queue, 2);
    push(output, &queue, 3);
    push(output, &queue, 4);
    push(output, &queue, 5);
    push(output, &queue, 6);
    shift(output, &queue);
    shift(output, &queue);
    shift(output, &queue);
    shift(output, &queue);
    shift(output, &queue);
    shift(output, &queue);
    shift(output, &queue);
    push(output, &queue, 7);
    shift(output, &queue);

    DP_queue_dispose(&queue);
    DP_output_free(output);
    FILE_EQ_OK("test/tmp/int_queue", "test/data/int_queue",
               "int queue operation log matches expected");
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(int_queue);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
