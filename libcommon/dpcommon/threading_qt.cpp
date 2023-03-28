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
extern "C" {
#include "common.h"
#include "threading.h"
// Qt doesn't have a way to get a numeric thread id, only a handle.
#if defined(__EMSCRIPTEN__) || defined(__APPLE__) || defined(__linux__)
#    include <pthread.h>
#elif defined(_WIN32)
#    include <windows.h>
#else
#    error "unknown platform"
#endif
}
#include <QByteArray>
#include <QMutex>
#include <QSemaphore>
#include <QSharedPointer>
#include <QThread>
#include <QThreadStorage>


extern "C" DP_Mutex *DP_mutex_new(void)
{
    return reinterpret_cast<DP_Mutex *>(new QMutex);
}

extern "C" void DP_mutex_free(DP_Mutex *mutex)
{
    delete reinterpret_cast<QMutex *>(mutex);
}

extern "C" bool DP_mutex_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    reinterpret_cast<QMutex *>(mutex)->lock();
    return true;
}

extern "C" DP_MutexResult DP_mutex_try_lock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    if (reinterpret_cast<QMutex *>(mutex)->tryLock()) {
        return DP_MUTEX_OK;
    }
    else {
        return DP_MUTEX_BLOCKED;
    }
}

extern "C" bool DP_mutex_unlock(DP_Mutex *mutex)
{
    DP_ASSERT(mutex);
    reinterpret_cast<QMutex *>(mutex)->unlock();
    return true;
}


extern "C" DP_Semaphore *DP_semaphore_new(unsigned int initial_value)
{
    return reinterpret_cast<DP_Semaphore *>(
        new QSemaphore{static_cast<int>(initial_value)});
}

extern "C" void DP_semaphore_free(DP_Semaphore *sem)
{
    delete reinterpret_cast<QSemaphore *>(sem);
}

extern "C" int DP_semaphore_value(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    return reinterpret_cast<QSemaphore *>(sem)->available();
}

extern "C" bool DP_semaphore_post(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    reinterpret_cast<QSemaphore *>(sem)->release();
    return true;
}

extern "C" bool DP_semaphore_post_n(DP_Semaphore *sem, int n)
{
    DP_ASSERT(sem);
    DP_ASSERT(n >= 0);
    reinterpret_cast<QSemaphore *>(sem)->release(n);
    return true;
}

extern "C" DP_SemaphoreResult DP_semaphore_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    reinterpret_cast<QSemaphore *>(sem)->acquire();
    return DP_SEMAPHORE_OK;
}

extern "C" int DP_semaphore_wait_n(DP_Semaphore *sem, int n)
{
    DP_ASSERT(sem);
    reinterpret_cast<QSemaphore *>(sem)->acquire(n);
    return n;
}

extern "C" DP_SemaphoreResult DP_semaphore_try_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    if (reinterpret_cast<QSemaphore *>(sem)->tryAcquire()) {
        return DP_SEMAPHORE_OK;
    }
    else {
        return DP_SEMAPHORE_BLOCKED;
    }
}


extern "C" unsigned long long DP_thread_current_id(void)
{
    // Qt doesn't have a way to get a numeric thread id, only a handle.
#if defined(__EMSCRIPTEN__) || defined(__APPLE__) || defined(__linux__)
    return (unsigned long long)pthread_self();
#elif defined(_WIN32)
    return GetCurrentThreadId();
#else
#    error "unknown platform"
#endif
}

extern "C" int DP_thread_cpu_count(void)
{
    return DP_max_int(1, QThread::idealThreadCount());
}

extern "C" DP_Thread *DP_thread_new(DP_ThreadFn fn, void *data)
{
    QThread *qthread = QThread::create([=]() { fn(data); });
    qthread->start();
    return reinterpret_cast<DP_Thread *>(qthread);
}

extern "C" void DP_thread_free_join(DP_Thread *thread)
{
    QThread *qthread = reinterpret_cast<QThread *>(thread);
    if (qthread) {
        if (!qthread->wait()) {
            DP_warn("Error joining thread");
        }
        delete qthread;
    }
}


class DP_QtErrorState final {
  public:
    DP_QtErrorState()
        : m_count{0}, m_buffer{DP_ERROR_STATE_INITIAL_BUFFER_SIZE, 0}
    {
    }

    DP_ErrorState get()
    {
        DP_ErrorState error = {&m_count, static_cast<size_t>(m_buffer.length()),
                               m_buffer.data()};
        return error;
    }

    DP_ErrorState resize(size_t size)
    {
        m_buffer.resize(static_cast<int>(size));
        return get();
    }

  private:
    unsigned int m_count;
    QByteArray m_buffer;
};

static QThreadStorage<DP_QtErrorState> thread_local_error_state;

extern "C" DP_ErrorState DP_thread_error_state_get(void)
{
    return thread_local_error_state.localData().get();
}

extern "C" DP_ErrorState DP_thread_error_state_resize(size_t new_size)
{
    return thread_local_error_state.localData().resize(new_size);
}
