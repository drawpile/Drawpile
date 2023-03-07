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
#include "cpu.h"
#include <stdlib.h>

DP_CpuSupport DP_cpu_support_value = DP_CPU_SUPPORT_DEFAULT;

#ifndef NDEBUG
static bool init_called = false;

DP_CpuSupport DP_cpu_support_debug_get(const char *file, int line)
{
    if (!init_called) {
        DP_warn_at(
            file, line,
            "DP_cpu_support used before DP_cpu_support_init() was called.");
    }

    return DP_cpu_support_value;
}
#endif

static DP_CpuSupport get_env_cpu_support(void)
{
    const char *value = getenv("DP_CPU_SUPPORT");
    if (value == NULL || DP_str_equal(value, "")) {
        return DP_CPU_SUPPORT_COUNT;
    }
    else if (DP_str_equal_lowercase(value, "default")) {
        DP_warn("Restricting CPU support to minimum defaults");
        return DP_CPU_SUPPORT_DEFAULT;
    }
#ifdef DP_CPU_X64
    else if (DP_str_equal_lowercase(value, "sse42")) {
        DP_warn("Restricting CPU support to at most SSE 4.2");
        return DP_CPU_SUPPORT_SSE42;
    }
    else if (DP_str_equal_lowercase(value, "avx")) {
        DP_warn("Restricting CPU support to at most AVX");
        return DP_CPU_SUPPORT_AVX;
    }
    else if (DP_str_equal_lowercase(value, "avx2")) {
        DP_warn("Restricting CPU support to at most AVX2");
        return DP_CPU_SUPPORT_AVX2;
    }
#endif
    else {
        DP_warn("Unknown DP_CPU_SUPPORT value '%s', ignoring it", value);
        return DP_CPU_SUPPORT_COUNT;
    }
}

#ifdef DP_CPU_X64

static bool supports_sse42(void)
{
#    if defined(__SSE4_2__)
    return true;
#    elif defined(_MSC_VER)
    int CPUInfo[4];
    __cpuid(CPUInfo, 1);
    return (CPUInfo[2] & (1 << 20)) != 0;
#    elif defined(__clang__) || defined(__GNUC__)
    return __builtin_cpu_supports("sse4.2");
#    else
    return false; // unknown compiler
#    endif
}

static bool supports_avx(void)
{
#    if defined(__AVX__)
    return true;
#    elif defined(_MSC_VER)
    int CPUInfo[4];
    __cpuid(CPUInfo, 1);
    return (CPUInfo[2] & (1 << 28)) != 0;
#    elif defined(__clang__) || defined(__GNUC__)
    return __builtin_cpu_supports("avx");
#    else
    return false; // unknown compiler
#    endif
}

static bool supports_avx2(void)
{
#    if defined(__AVX2__)
    return true;
#    elif defined(_MSC_VER)
    int CPUInfo[4];
    __cpuidex(CPUInfo, 7, 0);
    return (CPUInfo[1] & (1 << 5)) != 0;
#    elif defined(__clang__) || defined(__GNUC__)
    return __builtin_cpu_supports("avx2");
#    else
    return false; // unknown compiler
#    endif
}

#endif

void DP_cpu_support_init(void)
{
    DP_CpuSupport max_support = get_env_cpu_support();
#ifdef DP_CPU_X64
    if (max_support >= DP_CPU_SUPPORT_AVX2 && supports_avx2()) {
        DP_cpu_support_value = DP_CPU_SUPPORT_AVX2;
    }
    else if (max_support >= DP_CPU_SUPPORT_AVX && supports_avx()) {
        DP_cpu_support_value = DP_CPU_SUPPORT_AVX;
    }
    else if (max_support >= DP_CPU_SUPPORT_SSE42 && supports_sse42()) {
        DP_cpu_support_value = DP_CPU_SUPPORT_SSE42;
    }
    else {
        DP_cpu_support_value = DP_CPU_SUPPORT_DEFAULT;
    }
#else
    (void)max_support;
    DP_cpu_support_value = DP_CPU_SUPPORT_DEFAULT;
#endif

#ifndef NDEBUG
    init_called = true;
#endif
}
