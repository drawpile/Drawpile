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
#include "atomic.h"
#include "common.h"
#include "conversions.h"
#include "threading.h"
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>


struct DP_Mutex {
    pthread_mutex_t value;
};

struct DP_Semaphore {
    sem_t value;
};

struct DP_Thread {
    pthread_t value;
    DP_ThreadFn fn;
    void *data;
};

struct DP_ThreadRunArgs {
    DP_ThreadFn fn;
    void *data;
};


DP_Mutex *DP_mutex_new(void)
{
    DP_Mutex *mutex = DP_malloc(sizeof(*mutex));
    int error = pthread_mutex_init(&mutex->value, NULL);
    if (error == 0) {
        return mutex;
    }
    else {
        DP_free(mutex);
        DP_error_set("Can't create mutex: %s", strerror(error));
        return NULL;
    }
}

void DP_mutex_free(DP_Mutex *mutex)
{
    if (mutex) {
        int error = pthread_mutex_destroy(&mutex->value);
        if (error != 0) {
            DP_warn("Error destroying mutex: %s", strerror(error));
        }
        DP_free(mutex);
    }
}

bool DP_mutex_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    int error = pthread_mutex_lock(&mutex->value);
    if (error == 0) {
        return true;
    }
    else {
        DP_error_set("Can't lock mutex: %s", strerror(error));
        return false;
    }
}

DP_MutexResult DP_mutex_try_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    int error = pthread_mutex_trylock(&mutex->value);
    if (error == 0) {
        return DP_MUTEX_OK;
    }
    else {
        switch (error) {
        case EBUSY:
            return DP_MUTEX_BLOCKED;
        default:
            DP_error_set("Can't try lock mutex: %s", strerror(error));
            return DP_MUTEX_ERROR;
        }
    }
}

bool DP_mutex_unlock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    int error = pthread_mutex_unlock(&mutex->value);
    if (error == 0) {
        return true;
    }
    else {
        DP_error_set("Can't unlock mutex: %s", strerror(error));
        return false;
    }
}


DP_Semaphore *DP_semaphore_new(unsigned int initial_value)
{
    DP_Semaphore *sem = DP_malloc(sizeof(*sem));
    if (sem_init(&sem->value, false, initial_value) == 0) {
        return sem;
    }
    else {
        DP_error_set("Can't create semaphore: %s", strerror(errno));
        DP_free(sem);
        return NULL;
    }
}

void DP_semaphore_free(DP_Semaphore *sem)
{
    if (sem) {
        if (sem_destroy(&sem->value) != 0) {
            DP_warn("Error destroying semaphore: %s", strerror(errno));
        }
        DP_free(sem);
    }
}

int DP_semaphore_value(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    int sval;
    if (sem_getvalue(&sem->value, &sval)) {
        return sval > 0 ? sval : 0;
    }
    else {
        DP_error_set("Can't get semaphore value: %s", strerror(errno));
        return -1;
    }
}

bool DP_semaphore_post(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    if (sem_post(&sem->value) == 0) {
        return true;
    }
    else {
        DP_error_set("Can't post semaphore: %s", strerror(errno));
        return false;
    }
}

bool DP_semaphore_post_n(DP_Semaphore *sem, int n)
{
    DP_ASSERT(sem);
    DP_ASSERT(n >= 0);
    for (int i = 0; i < n; ++i) {
        if (sem_post(&sem->value) != 0) {
            DP_error_set("Can't post semaphore: %s", strerror(errno));
            return false;
        }
    }
    return true;
}

DP_SemaphoreResult DP_semaphore_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    if (sem_wait(&sem->value) == 0) {
        return DP_SEMAPHORE_OK;
    }
    else {
        int error = errno;
        switch (error) {
        case EINTR:
            return DP_SEMAPHORE_INTERRUPTED;
        default:
            DP_error_set("Can't wait for semaphore: %s", strerror(error));
            return DP_SEMAPHORE_ERROR;
        }
    }
}

int DP_semaphore_wait_n(DP_Semaphore *sem, int n)
{
    int i = 0;
    while (i < n) {
        if (DP_semaphore_wait(sem) == DP_SEMAPHORE_OK) {
            ++i;
        }
        else {
            break;
        }
    }
    return i;
}

DP_SemaphoreResult DP_semaphore_try_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    if (sem_trywait(&sem->value)) {
        return DP_SEMAPHORE_OK;
    }
    else {
        int error = errno;
        switch (error) {
        case EAGAIN:
            return DP_SEMAPHORE_BLOCKED;
        case EINTR:
            return DP_SEMAPHORE_INTERRUPTED;
        default:
            DP_error_set("Can't try wait for semaphore: %s", strerror(error));
            return DP_SEMAPHORE_ERROR;
        }
    }
}


unsigned long long DP_thread_current_id(void)
{
    return pthread_self();
}

int DP_thread_cpu_count(void)
{
    static int cpus;
    if (cpus == 0) {
        cpus = DP_max_int(1, DP_long_to_int(sysconf(_SC_NPROCESSORS_ONLN)));
    }
    return cpus;
}

static void *run_thread(void *arg)
{
    struct DP_ThreadRunArgs *run_args = arg;
    DP_ThreadFn fn = run_args->fn;
    void *data = run_args->data;
    DP_free(run_args);
    fn(data);
    return NULL;
}

DP_Thread *DP_thread_new(DP_ThreadFn fn, void *data)
{
    DP_Thread *thread = DP_malloc(sizeof(*thread));
    struct DP_ThreadRunArgs *run_args = DP_malloc(sizeof(*run_args));
    *run_args = (struct DP_ThreadRunArgs){fn, data};
    int error = pthread_create(&thread->value, NULL, run_thread, run_args);
    if (error == 0) {
        return thread;
    }
    else {
        DP_free(run_args);
        DP_free(thread);
        DP_error_set("Error creating thread: %s", strerror(error));
        return NULL;
    }
}

void DP_thread_free_join(DP_Thread *thread)
{
    if (thread) {
        int error = pthread_join(thread->value, NULL);
        if (error != 0) {
            DP_warn("Error joining thread: %s", strerror(error));
        }
        DP_free(thread);
    }
}


typedef struct DP_PthreadErrorState {
    unsigned int count;
    size_t buffer_size;
    char *buffer;
} DP_PthreadErrorState;

static void free_pthread_errror_state(void *arg)
{
    DP_PthreadErrorState *error = arg;
    DP_free(error->buffer);
    DP_free(error);
}

static DP_PthreadErrorState *get_pthread_error_state(void)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(lock);
    static pthread_key_t key;

    if (key == 0) {
        DP_atomic_lock(&lock);
        if (key == 0) {
            int error = pthread_key_create(&key, free_pthread_errror_state);
            if (error != 0) {
                DP_panic("Error creating thread-local key: %s",
                         strerror(error));
            }
        }
        DP_atomic_unlock(&lock);
    }

    DP_PthreadErrorState *state = pthread_getspecific(key);
    if (!state) {
        state = DP_malloc(sizeof(*state));
        *state = (DP_PthreadErrorState){
            0, DP_ERROR_STATE_INITIAL_BUFFER_SIZE,
            DP_malloc(DP_ERROR_STATE_INITIAL_BUFFER_SIZE)};

        int error = pthread_setspecific(key, state);
        if (error != 0) {
            DP_panic("Error setting thread-local: %s", strerror(error));
        }
    }

    return state;
}

static DP_ErrorState to_error_state(DP_PthreadErrorState *state)
{
    return (DP_ErrorState){&state->count, state->buffer_size, state->buffer};
}

DP_ErrorState DP_thread_error_state_get(void)
{
    return to_error_state(get_pthread_error_state());
}

DP_ErrorState DP_thread_error_state_resize(size_t new_size)
{
    DP_PthreadErrorState *state = get_pthread_error_state();
    state->buffer_size = new_size;
    state->buffer = DP_realloc(state->buffer, new_size);
    return to_error_state(state);
}
