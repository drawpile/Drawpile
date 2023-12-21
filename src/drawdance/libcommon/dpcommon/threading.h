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
#ifndef DPCOMMON_THREADING_H
#define DPCOMMON_THREADING_H
#include "common.h"


#define DP_ERROR_STATE_INITIAL_BUFFER_SIZE 128

#if defined(_WIN32)
typedef int DP_ThreadId;
#    define DP_THREAD_ID_FMT "d"
#elif defined(__ANDROID__)
typedef long DP_ThreadId;
#    define DP_THREAD_ID_FMT "ld"
#elif defined(__EMSCRIPTEN_PTHREADS__)
typedef void * DP_ThreadId;
#   define DP_THREAD_ID_FMT "p"
#else
typedef unsigned long long DP_ThreadId;
#    define DP_THREAD_ID_FMT "llu"
#endif

typedef struct DP_Mutex DP_Mutex;
typedef struct DP_Semaphore DP_Semaphore;
typedef struct DP_Thread DP_Thread;

typedef void (*DP_ThreadFn)(void *data);

typedef enum DP_MutexResult {
    DP_MUTEX_OK,
    DP_MUTEX_BLOCKED,
    DP_MUTEX_ERROR,
} DP_MutexResult;

typedef enum DP_SemaphoreResult {
    DP_SEMAPHORE_OK,
    DP_SEMAPHORE_BLOCKED,
    DP_SEMAPHORE_INTERRUPTED,
    DP_SEMAPHORE_ERROR,
} DP_SemaphoreResult;

typedef struct DP_ErrorState {
    unsigned int *count;
    size_t buffer_size;
    char *buffer;
} DP_ErrorState;


DP_Mutex *DP_mutex_new(void);

void DP_mutex_free(DP_Mutex *mutex);

bool DP_mutex_lock(DP_Mutex *mutex);

DP_MutexResult DP_mutex_try_lock(DP_Mutex *mutex);

bool DP_mutex_unlock(DP_Mutex *mutex);

void DP_mutex_must_lock_at(const char *file, int line, DP_Mutex *mutex);

bool DP_mutex_must_try_lock_at(const char *file, int line, DP_Mutex *mutex);

void DP_mutex_must_spin_lock_at(const char *file, int line, DP_Mutex *mutex);

#define DP_MUTEX_MUST_LOCK(MUTEX) \
    DP_mutex_must_lock_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, (MUTEX))

#define DP_MUTEX_MUST_TRY_LOCK(MUTEX)                                     \
    DP_mutex_must_try_lock_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, \
                              (MUTEX))

#define DP_MUTEX_MUST_UNLOCK(MUTEX)     \
    do {                                \
        if (!DP_mutex_unlock(MUTEX)) {  \
            DP_panic("%s", DP_error()); \
        }                               \
    } while (0)


DP_Semaphore *DP_semaphore_new(unsigned int initial_value);

void DP_semaphore_free(DP_Semaphore *sem);

int DP_semaphore_value(DP_Semaphore *sem);

bool DP_semaphore_post(DP_Semaphore *sem);

bool DP_semaphore_post_n(DP_Semaphore *sem, int n);

DP_SemaphoreResult DP_semaphore_wait(DP_Semaphore *sem);

int DP_semaphore_wait_n(DP_Semaphore *sem, int n);

DP_SemaphoreResult DP_semaphore_try_wait(DP_Semaphore *sem);

void DP_semaphore_must_wait_at(const char *file, int line, DP_Semaphore *sem);

void DP_semaphore_must_wait_at_n(const char *file, int line, DP_Semaphore *sem,
                                 int n);

bool DP_semaphore_must_try_wait_at(const char *file, int line,
                                   DP_Semaphore *sem);

#define DP_SEMAPHORE_MUST_POST(SEM)     \
    do {                                \
        if (!DP_semaphore_post(SEM)) {  \
            DP_panic("%s", DP_error()); \
        }                               \
    } while (0)

#define DP_SEMAPHORE_MUST_POST_N(SEM, N)    \
    do {                                    \
        if (!DP_semaphore_post_n(SEM, N)) { \
            DP_panic("%s", DP_error());     \
        }                                   \
    } while (0)

#define DP_SEMAPHORE_MUST_WAIT(SEM) \
    DP_semaphore_must_wait_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, (SEM))

#define DP_SEMAPHORE_MUST_WAIT_N(SEM, N)                                    \
    DP_semaphore_must_wait_at_n(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, \
                                (SEM), (N))

#define DP_SEMAPHORE_MUST_TRY_WAIT(SEM)                                       \
    DP_semaphore_must_try_wait_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, \
                                  (SEM))


DP_ThreadId DP_thread_current_id(void);

int DP_thread_cpu_count(int max);

DP_Thread *DP_thread_new(DP_ThreadFn fn, void *data);

void DP_thread_free_join(DP_Thread *thread);


DP_ErrorState DP_thread_error_state_get(void);

DP_ErrorState DP_thread_error_state_resize(size_t size);


#endif
