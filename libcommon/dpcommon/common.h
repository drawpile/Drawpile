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
#ifndef DPCOMMON_COMMON_H
#define DPCOMMON_COMMON_H
#include <assert.h>   // IWYU pragma: export
#include <stdalign.h> // IWYU pragma: export
#include <stdarg.h>   // IWYU pragma: export
#include <stdbool.h>  // IWYU pragma: export
#include <stddef.h>   // IWYU pragma: export
#include <stdint.h>   // IWYU pragma: export
#include <stdlib.h>   // IWYU pragma: export
#include <string.h>   // IWYU pragma: export


#ifdef __GNUC__
#    define DP_TRAP()        __builtin_trap()
#    define DP_UNUSED        __attribute__((__unused__))
#    define DP_UNREACHABLE() __builtin_unreachable()
#    define DP_FORMAT(STRING_INDEX, FIRST_TO_CHECK) \
        __attribute__((__format__(printf, STRING_INDEX, FIRST_TO_CHECK)))
#    ifdef __clang__
#        define DP_MALLOC_ATTR __attribute__((malloc, alloc_size(1)))
#    else
#        define DP_MALLOC_ATTR \
            __attribute__((malloc, malloc(DP_free, 1), alloc_size(1)))
#    endif
#    define DP_REALLOC_ATTR __attribute__((malloc, alloc_size(2)))
#    define DP_MUST_CHECK   __attribute((warn_unused_result))
#    define DP_INLINE       DP_UNUSED static inline
#else
#    define DP_TRAP()                               abort()
#    define DP_UNUSED                               // nothing
#    define DP_UNREACHABLE()                        // nothing
#    define DP_FORMAT(STRING_INDEX, FIRST_TO_CHECK) // nothing
#    define DP_MALLOC_ATTR                          // nothing
#    define DP_REALLOC_ATTR                         // nothing
#    define DP_MUST_CHECK                           // nothing
#endif

#ifdef NDEBUG
#    define DP_ASSERT(X) // nothing
#else
#    define DP_ASSERT(X) assert(X)
#endif

#ifdef __cplusplus
#    define DP_NORETURN [[noreturn]]
#else
#    define DP_NORETURN _Noreturn
#endif


#ifdef NDEBUG
#    define DP_debug(...) // nothing
#else
#    define DP_debug(...) \
        DP_debug_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, __VA_ARGS__)
void DP_debug_at(const char *file, int line, const char *fmt, ...)
    DP_FORMAT(3, 4);
#endif

#define DP_warn(...) \
    DP_warn_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, __VA_ARGS__)
void DP_warn_at(const char *file, int line, const char *fmt, ...)
    DP_FORMAT(3, 4);

#define DP_panic(...) \
    DP_panic_at(&__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__, __VA_ARGS__)
DP_NORETURN void DP_panic_at(const char *file, int line, const char *fmt, ...)
    DP_FORMAT(3, 4);


#define DP_MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define DP_MAX(X, Y) ((X) < (Y) ? (Y) : (X))


#define DP_ARRAY_LENGTH(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

DP_INLINE int DP_square_int(int x)
{
    return x * x;
}

DP_INLINE double DP_square_double(double x)
{
    return x * x;
}

DP_INLINE int DP_min_int(int x, int y)
{
    return x < y ? x : y;
}

DP_INLINE int DP_max_int(int x, int y)
{
    return x < y ? y : x;
}

DP_INLINE size_t DP_min_size(size_t x, size_t y)
{
    return x < y ? x : y;
}

DP_INLINE size_t DP_max_size(size_t x, size_t y)
{
    return x < y ? y : x;
}

DP_INLINE unsigned int DP_min_uint(unsigned int x, unsigned int y)
{
    return x < y ? x : y;
}

DP_INLINE unsigned int DP_max_uint(unsigned int x, unsigned int y)
{
    return x < y ? y : x;
}

DP_INLINE uint8_t DP_min_uint8(uint8_t x, uint8_t y)
{
    return x < y ? x : y;
}

DP_INLINE uint8_t DP_max_uint8(uint8_t x, uint8_t y)
{
    return x < y ? y : x;
}

DP_INLINE size_t DP_flex_size(size_t type_size, size_t flex_offset,
                              size_t flex_size, size_t count)
{
    return DP_max_size(type_size, flex_offset + flex_size * count);
}

/*
 * Overly correct way of getting the size of a structure with a flexible array
 * member. Takes potential trailing padding being used as part of the flexible
 * array member into account.
 */
#define DP_FLEX_SIZEOF(TYPE, FIELD, COUNT)            \
    DP_flex_size(sizeof(TYPE), offsetof(TYPE, FIELD), \
                 sizeof(((TYPE *)NULL)->FIELD[0]), COUNT)


void DP_free(void *ptr);

void *DP_malloc(size_t size) DP_MALLOC_ATTR;

void *DP_realloc(void *ptr, size_t size) DP_REALLOC_ATTR;


char *DP_vformat(const char *fmt, va_list ap);

char *DP_format(const char *fmt, ...) DP_FORMAT(1, 2);

char *DP_strdup(const char *str);


void *DP_slurp(const char *path, size_t *out_length);


const char *DP_error(void);

const char *DP_error_since(unsigned int count);

void DP_error_set(const char *fmt, ...) DP_FORMAT(1, 2);

unsigned int DP_error_count(void);

void DP_error_count_set(unsigned int count);

unsigned int DP_error_count_since(unsigned int count);


#endif
