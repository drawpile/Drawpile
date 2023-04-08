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
#ifndef DPCOMMON_ATOMIC_H
#define DPCOMMON_ATOMIC_H
#include "common.h"

#ifdef _MSC_VER
#    include <windows.h>

typedef LONG64 volatile DP_Atomic;
#    define DP_ATOMIC_INIT(X) X

#    define DP_atomic_get(X)        ((int)(*(X)))
#    define DP_atomic_set(X, VALUE) ((void)InterlockedExchange64((X), (VALUE)))
#    define DP_atomic_xch(X, VALUE) ((int)InterlockedExchange64((X), (VALUE)))
#    define DP_atomic_add(X, VALUE) ((void)_InlineInterlockedAdd64((X), VALUE))
#    define DP_atomic_inc(X)        ((void)InterlockedIncrement64((X)))
#    define DP_atomic_dec(X)        (InterlockedDecrement64((X)) == 0)

DP_INLINE bool DP_atomic_compare_exchange(DP_Atomic *x, int expected,
                                          int desired)
{
    return InterlockedCompareExchange64(x, (LONG64)desired, (LONG64)expected)
        == (LONG64)expected;
}

typedef void *volatile DP_AtomicPtr;

#    define DP_ATOMIC_PTR_INIT(X) X
#    define DP_atomic_ptr_set(X, VALUE) \
        ((void)InterlockedExchangePointer((X), (VALUE)))
#    define DP_atomic_ptr_xch(X, VALUE) InterlockedExchangePointer((X), (VALUE))

#else
#    include <stdatomic.h>

typedef atomic_int DP_Atomic;

#    define DP_ATOMIC_INIT(X) X

#    define DP_atomic_get(X)        atomic_load((X))
#    define DP_atomic_set(X, VALUE) atomic_store((X), (VALUE))
#    define DP_atomic_xch(X, VALUE) atomic_exchange((X), (VALUE))
#    define DP_atomic_add(X, VALUE) ((void)atomic_fetch_add((X), VALUE))
#    define DP_atomic_inc(X)        DP_atomic_add((X), 1)
#    define DP_atomic_dec(X)        (atomic_fetch_sub((X), 1) == 1)

DP_INLINE bool DP_atomic_compare_exchange(DP_Atomic *x, int expected,
                                          int desired)
{
    return atomic_compare_exchange_weak_explicit(
        x, &expected, desired, memory_order_seq_cst, memory_order_relaxed);
}

typedef _Atomic(void *) DP_AtomicPtr;

#    define DP_ATOMIC_PTR_INIT(X)       X
#    define DP_atomic_ptr_set(X, VALUE) atomic_store((X), (VALUE))
#    define DP_atomic_ptr_xch(X, VALUE) atomic_exchange((X), (VALUE))

#endif

#define DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(NAME) static DP_Atomic NAME

void DP_atomic_lock(DP_Atomic *x);

void DP_atomic_unlock(DP_Atomic *x);

#endif
