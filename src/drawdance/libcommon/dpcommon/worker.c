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
#include "worker.h"
#include "common.h"
#include "conversions.h"
#include "queue.h"
#include "threading.h"


typedef struct DP_Worker {
    size_t element_size;
    DP_WorkerJobFn job_fn;
    DP_Semaphore *sem;
    DP_Queue queue;
    DP_Mutex *queue_mutex;
    int thread_count;
    DP_Thread *threads[];
} DP_Worker;

struct DP_WorkerParams {
    DP_Worker *worker;
    int thread_index;
};


int DP_worker_cpu_count(int max)
{
#ifdef __EMSCRIPTEN__
    // In the browser, "thread" overhead for web workers is very high. On mobile
    // devices, especially iOS, we're also under memory pressure. So we'll keep
    // it at one thread per worker.
    return DP_min_int(1, max);
#else
    return DP_thread_cpu_count(max);
#endif
}


static bool shift_worker_element(DP_Mutex *queue_mutex, DP_Queue *queue,
                                 size_t element_size, void *out_element)
{
    DP_MUTEX_MUST_LOCK(queue_mutex);
    void *element = DP_queue_peek(queue, element_size);
    if (element) {
        memcpy(out_element, element, element_size);
        DP_queue_shift(queue);
    }
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
    return element;
}

static void run_worker_thread(void *data)
{
    struct DP_WorkerParams *params = data;
    DP_Worker *worker = params->worker;
    int thread_index = params->thread_index;
    DP_free(params);

    size_t element_size = worker->element_size;
    DP_WorkerJobFn job_fn = worker->job_fn;
    DP_Semaphore *sem = worker->sem;
    DP_Mutex *queue_mutex = worker->queue_mutex;
    DP_Queue *queue = &worker->queue;
    void *element = DP_malloc(element_size);
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        if (shift_worker_element(queue_mutex, queue, element_size, element)) {
            job_fn(element, thread_index);
        }
        else {
            break;
        }
    }
    DP_free(element);
}

DP_Worker *DP_worker_new(size_t initial_capacity, size_t element_size,
                         int thread_count, DP_WorkerJobFn job_fn)
{
    DP_ASSERT(initial_capacity > 0);
    DP_ASSERT(element_size > 0);
    DP_ASSERT(thread_count > 0);
    DP_ASSERT(job_fn);

    DP_Worker *worker = DP_malloc_zeroed(
        DP_FLEX_SIZEOF(DP_Worker, threads, DP_int_to_size(thread_count)));
    worker->element_size = element_size;
    worker->job_fn = job_fn;

    worker->sem = DP_semaphore_new(0);
    if (!worker->sem) {
        DP_worker_free_join(worker);
        return NULL;
    }

    DP_queue_init(&worker->queue, initial_capacity, element_size);

    worker->queue_mutex = DP_mutex_new();
    if (!worker->queue_mutex) {
        DP_worker_free_join(worker);
        return NULL;
    }

    for (int i = 0; i < thread_count; ++i) {
        struct DP_WorkerParams *params = DP_malloc(sizeof(*params));
        *params = (struct DP_WorkerParams){worker, i};
        DP_Thread *thread = DP_thread_new(run_worker_thread, params);
        if (thread) {
            worker->threads[i] = thread;
            worker->thread_count = i + 1;
        }
        else {
            DP_free(params);
            DP_worker_free_join(worker);
            return NULL;
        }
    }

    return worker;
}

void DP_worker_free_join(DP_Worker *worker)
{
    if (worker) {
        DP_Semaphore *sem = worker->sem;
        int thread_count = worker->thread_count;
        if (sem) {
            DP_SEMAPHORE_MUST_POST_N(sem, thread_count);
        }
        for (int i = 0; i < thread_count; ++i) {
            DP_thread_free_join(worker->threads[i]);
        }
        DP_mutex_free(worker->queue_mutex);
        DP_queue_dispose(&worker->queue);
        DP_semaphore_free(sem);
        DP_free(worker);
    }
}

int DP_worker_thread_count(DP_Worker *worker)
{
    DP_ASSERT(worker);
    return worker->thread_count;
}

void DP_worker_push(DP_Worker *worker, void *element)
{
    DP_ASSERT(worker);
    DP_ASSERT(element);
    DP_Mutex *queue_mutex = worker->queue_mutex;
    size_t element_size = worker->element_size;
    DP_MUTEX_MUST_LOCK(queue_mutex);
    memcpy(DP_queue_push(&worker->queue, element_size), element, element_size);
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
    DP_SEMAPHORE_MUST_POST(worker->sem);
}
