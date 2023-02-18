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
#ifndef DPCOMMON_CPU_H
#define DPCOMMON_CPU_H
#include "common.h"

#if defined(_M_X64) || defined(__x86_64__)
#    define DP_CPU_X64
#endif

// Include intrinsics
#ifdef DP_CPU_X64
#    if defined(__clang__) && defined(_MSC_VER)
// If clang is used on windows, it'll skip instrinsic headers if the
// corresponding macros are not defined
#        ifndef __SSE3__
#            define __SSE3__
#            define UNDEF__SSE3__
#        endif
#        ifndef __SSSE3__
#            define __SSSE3__
#            define UNDEF__SSSE3__
#        endif
#        ifndef __SSE4_1__
#            define __SSE4_1__
#            define UNDEF__SSE4_1__
#        endif
#        ifndef __SSE4_2__
#            define __SSE4_2__
#            define UNDEF__SSE4_2__
#        endif
#        ifndef __AVX__
#            define __AVX__
#            define UNDEF__AVX__
#        endif
#        ifndef __AVX2__
#            define __AVX2__
#            define UNDEF__AVX2__
#        endif

#        include <intrin.h>

#        ifdef UNDEF__SSE3__
#            undef __SSE3__
#            undef UNDEF__SSE3__
#        endif
#        ifdef UNDEF__SSSE3__
#            undef __SSSE3__
#            undef UNDEF__SSSE3__
#        endif
#        ifdef UNDEF__SSE4_1__
#            undef __SSE4_1__
#            undef UNDEF__SSE4_1__
#        endif
#        ifdef UNDEF__SSE4_2__
#            undef __SSE4_2__
#            undef UNDEF__SSE4_2__
#        endif
#        ifdef UNDEF__AVX__
#            undef __AVX__
#            undef UNDEF__AVX__
#        endif
#        ifdef UNDEF__AVX2__
#            undef __AVX2__
#            undef UNDEF__AVX2__
#        endif
#    elif defined(__clang__) || defined(__GNUC__)
#        include <x86intrin.h>
#    else
#        include <intrin.h>
#    endif
#endif

#define DP_DO_PRAGMA_(x) _Pragma(#x)
#define DP_DO_PRAGMA(x)  DP_DO_PRAGMA_(x)

// clang-format off
#if defined(__clang__)
#    define DP_TARGET_BEGIN(TARGET) DP_DO_PRAGMA(clang attribute push(__attribute__((target(TARGET))), apply_to=function))
#    define DP_TARGET_END _Pragma("clang attribute pop")
#elif defined(__GNUC__)
#    define DP_TARGET_BEGIN(TARGET) _Pragma("GCC push_options") DP_DO_PRAGMA(GCC target(TARGET))
#    define DP_TARGET_END _Pragma("GCC pop_options")
#else
#    define DP_TARGET_BEGIN(TARGET) // nothing
#    define DP_TARGET_END           // nothing
#endif
// clang-format on

// Order matters
typedef enum DP_CpuSupport {
    DP_CPU_SUPPORT_DEFAULT,
#ifdef DP_CPU_X64
    DP_CPU_SUPPORT_SSE42,
    DP_CPU_SUPPORT_AVX2,
#endif
} DP_CpuSupport;

extern DP_CpuSupport DP_cpu_support_value; // Use DP_cpu_support macro instead
void DP_cpu_support_init(void);

#ifdef NDEBUG
#    define DP_cpu_support DP_cpu_support_value
#else
DP_CpuSupport DP_cpu_support_debug_get(const char *file, int line);
#    define DP_cpu_support \
        DP_cpu_support_debug_get(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__)
#endif


#endif
