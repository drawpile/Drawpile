/*
 * Copyright (c) 2023 askmeaboutloom
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
#ifndef DPCOMMON_PERF_H
#define DPCOMMON_PERF_H
#include "common.h"

typedef struct DP_Output DP_Output;

#define DP_PERF_VERSION        "2"
#define DP_PERF_INVALID_HANDLE (-1)

// Some preprocessor juggling because we want to use the
// macro expansion of DP_PERF_REALM as a string literal.
#define DP_PERF_STR(X)  #X
#define DP_PERF_XSTR(X) DP_PERF_STR(X)

// In C++ code, an RAII wrapper is more convenient.
// Disable these to avoid accidental usage of them.
#ifndef DP_PERF_NO_BEGIN_END
#    define DP_PERF_BEGIN(TARGET, CATEGORIES)      \
        int _perf_handle_##TARGET = DP_perf_begin( \
            DP_PERF_XSTR(DP_PERF_REALM), DP_PERF_CONTEXT ":" CATEGORIES, NULL)

#    define DP_PERF_BEGIN_DETAIL(TARGET, CATEGORIES, ...) \
        int _perf_handle_##TARGET =                       \
            DP_perf_begin(DP_PERF_XSTR(DP_PERF_REALM),    \
                          DP_PERF_CONTEXT ":" CATEGORIES, __VA_ARGS__)

#    define DP_PERF_END(TARGET) DP_perf_end(_perf_handle_##TARGET)
#endif


extern DP_Output *DP_perf_output;

// Takes ownership of the output, it is freed even on error.
bool DP_perf_open(DP_Output *output);
bool DP_perf_close(void);
bool DP_perf_is_open(void);

DP_INLINE int DP_perf_begin(const char *realm, const char *categories,
                            const char *fmt_or_null, ...) DP_FORMAT(3, 4);

int DP_perf_begin_internal(const char *realm, const char *categories,
                           const char *fmt, va_list ap) DP_VFORMAT(3);

DP_INLINE int DP_perf_begin(const char *realm, const char *categories,
                            const char *fmt_or_null, ...)
{
    if (DP_perf_output) {
        va_list ap;
        va_start(ap, fmt_or_null);
        int handle = DP_perf_begin_internal(realm, categories, fmt_or_null, ap);
        va_end(ap);
        return handle;
    }
    else {
        return DP_PERF_INVALID_HANDLE;
    }
}

void DP_perf_end_internal(DP_Output *output, int handle);

DP_INLINE void DP_perf_end(int handle)
{
    if (handle != DP_PERF_INVALID_HANDLE) {
        DP_Output *output = DP_perf_output;
        if (output) {
            DP_perf_end_internal(output, handle);
        }
    }
}


#endif
