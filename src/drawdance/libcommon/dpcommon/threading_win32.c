/*
 * Copyright (c) 2022
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

#include "common.h"
#include "threading.h"
#include <windows.h>

static void free_error_state(void);

DP_Mutex *DP_mutex_new(void)
{
    CRITICAL_SECTION *cs = DP_malloc(sizeof(*cs));
    InitializeCriticalSection(cs);
    return (DP_Mutex *)cs;
}

void DP_mutex_free(DP_Mutex *mutex)
{
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)mutex;
    if (cs) {
        DeleteCriticalSection(cs);
        DP_free(cs);
    }
}

bool DP_mutex_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    EnterCriticalSection((CRITICAL_SECTION *)mutex);
    return true;
}

DP_MutexResult DP_mutex_try_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    if (TryEnterCriticalSection((CRITICAL_SECTION *)mutex)) {
        return DP_MUTEX_OK;
    }
    else {
        return DP_MUTEX_BLOCKED;
    }
}

bool DP_mutex_unlock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    LeaveCriticalSection((CRITICAL_SECTION *)mutex);
    return true;
}


DP_Semaphore *DP_semaphore_new(unsigned int initial_value)
{
    return (DP_Semaphore *)CreateSemaphoreW(NULL, initial_value, LONG_MAX,
                                            NULL);
}

void DP_semaphore_free(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    CloseHandle(sem);
}

int DP_semaphore_value(DP_Semaphore *sem)
{
    DP_ASSERT(sem);

    // Win32 semaphores do not have an API to easily grab the value.
    // Only ReleaseSemaphore can return the previous count.
    // To avoid disturbing threads using the sempahore, it checks if it can
    // acquire it without waiting, and if it can, immediatly increase back the
    // sempahore, grabbing the value in the proccess.
    LONG count = 0;
    if (WaitForSingleObject(sem, 0L) == WAIT_OBJECT_0) {
        ReleaseSemaphore(sem, 1, &count);
        count++;
    }

    return count;
}

bool DP_semaphore_post(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    ReleaseSemaphore(sem, 1, NULL);
    return true;
}

bool DP_semaphore_post_n(DP_Semaphore *sem, int n)
{
    DP_ASSERT(sem);
    DP_ASSERT(n >= 0);
    ReleaseSemaphore(sem, n, NULL);
    return true;
}

DP_SemaphoreResult DP_semaphore_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    WaitForSingleObject(sem, INFINITE);
    return DP_SEMAPHORE_OK;
}

int DP_semaphore_wait_n(DP_Semaphore *sem, int n)
{
    DP_ASSERT(sem);
    for (int i = 0; i < n; i++) {
        WaitForSingleObject(sem, INFINITE);
    }
    return n;
}

DP_SemaphoreResult DP_semaphore_try_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    if (WaitForSingleObject(sem, 0) == WAIT_OBJECT_0) {
        return DP_SEMAPHORE_OK;
    }
    else {
        return DP_SEMAPHORE_BLOCKED;
    }
}

DP_ThreadId DP_thread_current_id(void)
{
    return GetCurrentThreadId();
}

int DP_thread_cpu_count(void)
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return DP_max_int(1, sysinfo.dwNumberOfProcessors);
}

// Wrapper function is needed to change return type. Casting function ptr is UB.
typedef struct RunThreadArgs {
    DP_ThreadFn fn;
    void *arg;
} RunThreadArgs;

static DWORD WINAPI run_thread(void *fn_arg)
{
    DP_ThreadFn fn = ((RunThreadArgs *)fn_arg)->fn;
    void *arg = ((RunThreadArgs *)fn_arg)->arg;
    DP_free(fn_arg);

    fn(arg);

    free_error_state();
    return 0;
}

DP_Thread *DP_thread_new(DP_ThreadFn fn, void *data)
{
    RunThreadArgs *run_args = DP_malloc(sizeof(RunThreadArgs));
    run_args->fn = fn;
    run_args->arg = data;

    return (DP_Thread *)CreateThread(NULL, 0, run_thread, run_args, 0, NULL);
}

void DP_thread_free_join(DP_Thread *thread)
{
    if (thread) {
        WaitForSingleObject((HANDLE *)thread, INFINITE);
        CloseHandle((HANDLE *)thread);
    }
}

#ifdef _MSC_VER
#    define THREAD_LOCAL __declspec(thread)
#else
#    define THREAD_LOCAL __thread
#endif

static THREAD_LOCAL unsigned int error_state_count = 0;
static THREAD_LOCAL size_t error_state_buffer_size = 0;
static THREAD_LOCAL char *error_state_buffer = NULL;

static void free_error_state(void)
{
    DP_free(error_state_buffer);
}

DP_ErrorState DP_thread_error_state_get(void)
{
    if (error_state_buffer == NULL) {
        error_state_buffer =
            DP_malloc_zeroed(DP_ERROR_STATE_INITIAL_BUFFER_SIZE);
        error_state_buffer_size = DP_ERROR_STATE_INITIAL_BUFFER_SIZE;
    }

    return (DP_ErrorState){&error_state_count, error_state_buffer_size,
                           error_state_buffer};
}

DP_ErrorState DP_thread_error_state_resize(size_t new_size)
{
    if (error_state_buffer == NULL) {
        error_state_buffer = DP_malloc(new_size);
    }
    else {
        error_state_buffer = DP_realloc(error_state_buffer, new_size);
    }
    error_state_buffer_size = new_size;

    return (DP_ErrorState){&error_state_count, error_state_buffer_size,
                           error_state_buffer};
}
