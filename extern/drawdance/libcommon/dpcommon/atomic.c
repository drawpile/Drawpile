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

#if defined(_WIN32)
#    include <windows.h>
#    define DP_YIELD() YieldProcessor()
#else
#    include <sched.h>
#    define DP_YIELD() sched_yield()
#endif

void DP_atomic_lock(DP_Atomic *x)
{
    while (true) {
        bool locked = DP_atomic_compare_exchange(x, 0, 1);
        if (locked) {
            break;
        }
        else {
            DP_YIELD();
        }
    }
}

void DP_atomic_unlock(DP_Atomic *x)
{
#ifdef NDEBUG
    DP_atomic_set(x, 0);
#else
    int prev = DP_atomic_xch(x, 0);
    DP_ASSERT(prev != 0); // Make sure that a lock was actually present.
#endif
}
