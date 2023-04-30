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
#include "atomic.h"
#include "conversions.h"
#include "cpu.h"
#include "threading.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>


static const char *log_level_to_string(DP_LogLevel level)
{
    switch (level) {
    case DP_LOG_DEBUG:
        return "DEBUG";
    case DP_LOG_INFO:
        return "INFO";
    case DP_LOG_WARN:
        return "WARN";
    case DP_LOG_PANIC:
        return "PANIC";
    default:
        return "UNKNOWN";
    }
}

static void log_message(DP_UNUSED void *user, DP_LogLevel level,
                        const char *file, int line, const char *fmt, va_list ap)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(log_lock);
    DP_atomic_lock(&log_lock);
    fprintf(stderr, "[%s] %s:%d - ", log_level_to_string(level), file, line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    DP_atomic_unlock(&log_lock);
}

static DP_LogFn log_fn = log_message;
static void *log_user;

void *DP_log_fn_set(DP_LogFn fn, void *user)
{
    log_fn = fn;
    void *prev_user = user;
    log_user = user;
    return prev_user;
}

#define DO_LOG(LEVEL, FILE, LINE, FMT, AP)                             \
    do {                                                               \
        if (log_fn) {                                                  \
            va_list AP;                                                \
            va_start(AP, FMT);                                         \
            log_fn(log_user, LEVEL, FILE ? FILE : "?", LINE, FMT, AP); \
            va_end(AP);                                                \
        }                                                              \
    } while (0)

#ifndef NDEBUG
void DP_debug_at(const char *file, int line, const char *fmt, ...)
{
    DO_LOG(DP_LOG_DEBUG, file, line, fmt, ap);
}
#endif

void DP_info_at(const char *file, int line, const char *fmt, ...)
{
    DO_LOG(DP_LOG_INFO, file, line, fmt, ap);
}

void DP_warn_at(const char *file, int line, const char *fmt, ...)
{
    DO_LOG(DP_LOG_WARN, file, line, fmt, ap);
}

void DP_panic_at(const char *file, int line, const char *fmt, ...)
{
    DO_LOG(DP_LOG_PANIC, file, line, fmt, ap);
    DP_TRAP();
}


void *DP_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr || size == 0) {
        return ptr;
    }
    else {
        fprintf(stderr, "Allocation of %zu bytes failed\n", size);
        DP_TRAP();
    }
}

void *DP_malloc_zeroed(size_t size)
{
    void *ptr = calloc(size, 1);
    if (ptr || size == 0) {
        return ptr;
    }
    else {
        fprintf(stderr, "Zeroed allocation of %zu bytes failed\n", size);
        DP_TRAP();
    }
}

void *DP_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (new_ptr || size == 0) {
        return new_ptr;
    }
    else {
        fprintf(stderr, "Reallocation of %zu bytes failed\n", size);
        DP_TRAP();
    }
}

void DP_free(void *ptr)
{
    free(ptr);
}

#ifdef DP_SIMD_ALIGNMENT
void *DP_malloc_simd(size_t size)
{
    DP_ASSERT(size != 0);
#    ifdef _WIN32
    void *ptr = _aligned_malloc(size, DP_SIMD_ALIGNMENT);
    if (ptr) {
        return ptr;
    }
#    else
    void *ptr;
    if (posix_memalign(&ptr, DP_SIMD_ALIGNMENT, size) == 0) {
        return ptr;
    }
#    endif
    fprintf(stderr, "Allocation of %zu bytes with alignment %d failed\n", size,
            DP_SIMD_ALIGNMENT);
    DP_TRAP();
}

void *DP_malloc_simd_zeroed(size_t size)
{
    void *ptr = DP_malloc_simd(size);
    memset(ptr, 0, size);
    return ptr;
}
#endif

#if defined(DP_SIMD_ALIGNMENT) && defined(_WIN32)
void DP_free_simd(void *ptr)
{
    _aligned_free(ptr);
}
#endif


char *DP_vformat(const char *fmt, va_list ap)
{
    va_list aq;

    va_copy(aq, ap);
    int len = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);

    size_t size = DP_int_to_size(len) + 1;
    char *buf = DP_malloc(size);
    vsnprintf(buf, size, fmt, ap);

    return buf;
}

char *DP_format(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *buf = DP_vformat(fmt, ap);
    va_end(ap);
    return buf;
}

char *DP_strdup(const char *str)
{
    if (str) {
        size_t size = strlen(str) + 1;
        char *dup = DP_malloc(size);
        return strncpy(dup, str, size);
    }
    else {
        return NULL;
    }
}


bool DP_str_equal(const char *a, const char *b)
{
    return a == b || (a && strcmp(a, b) == 0);
}

bool DP_str_equal_lowercase(const char *a, const char *b)
{
    if (a == b) {
        return true;
    }

    if (a && b) {
        size_t len = strlen(a);
        if (len == strlen(b)) {
            for (size_t i = 0; i < len; ++i) {
                if (tolower(a[i]) != tolower(b[i])) {
                    return false;
                }
            }
            return true;
        }
    }

    return false;
}


const char *DP_error(void)
{
    DP_ErrorState error = DP_thread_error_state_get();
    return error.buffer;
}

const char *DP_error_since(unsigned int count)
{
    DP_ErrorState error = DP_thread_error_state_get();
    return count != *error.count ? error.buffer : NULL;
}

void DP_error_set(const char *fmt, ...)
{
    DP_ErrorState error = DP_thread_error_state_get();
    ++*error.count;

    va_list ap;
    va_start(ap, fmt);
    int length = vsnprintf(error.buffer, error.buffer_size, fmt, ap);
    va_end(ap);

    if (length >= 0) {
        size_t size = DP_int_to_size(length) + 1u;
        if (size > error.buffer_size) {
            error = DP_thread_error_state_resize(size);
            va_start(ap, fmt);
            vsnprintf(error.buffer, error.buffer_size, fmt, ap);
            va_end(ap);
        }
    }

    DP_debug("Set error %u: %s", *error.count,
             error.buffer ? error.buffer : "");
}

unsigned int DP_error_count(void)
{
    DP_ErrorState error = DP_thread_error_state_get();
    return *error.count;
}

void DP_error_count_set(unsigned int count)
{
    DP_ErrorState error = DP_thread_error_state_get();
    *error.count = count;
}

unsigned int DP_error_count_since(unsigned int count)
{
    unsigned int error_count = DP_error_count();
    if (error_count >= count) {
        return error_count - count;
    }
    else {
        // Wraparound of the unsigned int. The wraparound is guaranteed
        // for unsigned numbers by the C standard, so we can rely on it.
        return UINT_MAX - count + error_count + 1;
    }
}
