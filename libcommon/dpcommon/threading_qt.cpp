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
#define DP_INSIDE_EXTERN_C
#include "common.h"
#include "threading.h"
#undef DP_INSIDE_EXTERN_C
}
#include <QAtomicInteger>
#include <QHash>
#include <QMutex>
#include <QSemaphore>
#include <QSharedPointer>
#include <QThread>
#include <QThreadStorage>
#include <QVector>


typedef class DP_TlsValue DP_TlsValue;
typedef QHash<DP_TlsKey, QSharedPointer<DP_TlsValue>> DP_TlsHash;


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

extern "C" bool DP_semaphore_post(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    reinterpret_cast<QSemaphore *>(sem)->release();
    return true;
}

extern "C" DP_SemaphoreResult DP_semaphore_wait(DP_Semaphore *sem)
{
    DP_ASSERT(sem);
    reinterpret_cast<QSemaphore *>(sem)->acquire();
    return DP_SEMAPHORE_OK;
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


extern "C" DP_Thread *DP_thread_new(DP_ThreadFn fn, void *data)
{
    QThread *qthread = QThread::create(fn, data);
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


class DP_TlsValue {
  public:
    DP_TlsValue() : m_value(nullptr), m_destructor(nullptr)
    {
    }

    ~DP_TlsValue()
    {
        if (m_destructor) {
            m_destructor(m_value);
        }
    }

    void set(void *value, void (*destructor)(void *))
    {
        m_value = value;
        m_destructor = destructor;
    }

    void *value() const
    {
        return m_value;
    }

  private:
    Q_DISABLE_COPY(DP_TlsValue)
    void *m_value;
    void (*m_destructor)(void *);
};

static QAtomicInteger<int> tls_spinlock{0};
static QVector<void (*)(void *)> tls_destructors;
// The QHash interface requires copying stuff around, so a shared pointer is
// required, even though we're not sharing anything. C++ and all that.
static QThreadStorage<DP_TlsHash> tls;

static void with_tls_destructors(std::function<void()> fn)
{
    while (true) {
        bool locked = tls_spinlock.testAndSetOrdered(0, 1);
        if (locked) {
            break;
        }
        else {
            QThread::yieldCurrentThread();
        }
    }
    fn();
    tls_spinlock.storeRelease(0);
}

extern "C" DP_TlsKey DP_tls_create(void (*destructor)(void *))
{
    DP_TlsKey key;
    with_tls_destructors([&]() {
        tls_destructors.append(destructor);
        key = static_cast<DP_TlsKey>(tls_destructors.count());
    });
    return key;
}

extern "C" void *DP_tls_get(DP_TlsKey key)
{
    DP_ASSERT(key != DP_TLS_UNDEFINED);
    DP_TlsHash &store = tls.localData();
    DP_TlsHash::iterator it = store.find(key);
    if (it != store.end()) {
        return it->get()->value();
    }
    else {
        return nullptr;
    }
}

extern "C" void DP_tls_set(DP_TlsKey key, void *value)
{
    DP_ASSERT(key != DP_TLS_UNDEFINED);
    void (*destructor)(void *);
    with_tls_destructors([&]() {
        int i = static_cast<int>(key) - 1;
        DP_ASSERT(i >= 0);
        DP_ASSERT(i < tls_destructors.count());
        destructor = tls_destructors.at(i);
    });
    DP_TlsHash &store = tls.localData();
    DP_TlsHash::iterator it = store.find(key);
    DP_TlsValue *tls_value;
    if (it != store.end()) {
        tls_value = it->get();
    }
    else {
        tls_value = new DP_TlsValue;
        store.insert(key, QSharedPointer<DP_TlsValue>{tls_value});
    }
    tls_value->set(value, destructor);
}
