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
#include "common.h"
#include "threading.h"


void DP_mutex_must_lock_at(const char *file, int line, DP_Mutex *mutex)
{
    if (!DP_mutex_lock(mutex)) {
        DP_panic_at(file, line, "%s", DP_error());
    }
}

bool DP_mutex_must_try_lock_at(const char *file, int line, DP_Mutex *mutex)
{
    while (true) {
        switch (DP_mutex_try_lock(mutex)) {
        case DP_MUTEX_OK:
            return true;
        case DP_MUTEX_BLOCKED:
            return false;
        default:
            DP_panic_at(file, line, "%s", DP_error());
        }
    }
}


void DP_semaphore_must_wait_at(const char *file, int line, DP_Semaphore *sem)
{
    while (true) {
        switch (DP_semaphore_wait(sem)) {
        case DP_SEMAPHORE_OK:
            return;
        case DP_SEMAPHORE_INTERRUPTED:
            break;
        default:
            DP_panic_at(file, line, "%s", DP_error());
        }
    }
}

void DP_semaphore_must_wait_at_n(const char *file, int line, DP_Semaphore *sem,
                                 int n)
{
    if (DP_semaphore_wait_n(sem, n) != n) {
        DP_panic_at(file, line, "%s", DP_error());
    }
}

bool DP_semaphore_must_try_wait_at(const char *file, int line,
                                   DP_Semaphore *sem)
{
    while (true) {
        switch (DP_semaphore_try_wait(sem)) {
        case DP_SEMAPHORE_OK:
            return true;
        case DP_SEMAPHORE_BLOCKED:
            return false;
        case DP_SEMAPHORE_INTERRUPTED:
            break;
        default:
            DP_panic_at(file, line, "%s", DP_error());
        }
    }
}
