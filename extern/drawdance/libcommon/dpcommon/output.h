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
#ifndef DPCOMMON_OUTPUT_H
#define DPCOMMON_OUTPUT_H
#include "common.h"
#include <stdio.h>

#define DP_OUTPUT_PRINT_LITERAL(OUTPUT, LITERAL) \
    DP_output_write((OUTPUT), "" LITERAL, strlen(LITERAL))


typedef struct DP_Output DP_Output;

typedef struct DP_OutputMethods {
    size_t (*write)(void *internal, const void *buffer, size_t size);
    bool (*clear)(void *internal);
    bool (*flush)(void *internal);
    size_t (*tell)(void *internal, bool *out_error);
    bool (*seek)(void *internal, size_t offset);
    bool (*dispose)(void *internal);
} DP_OutputMethods;

typedef const DP_OutputMethods *(*DP_OutputInitFn)(void *internal, void *arg);

DP_Output *DP_output_new(DP_OutputInitFn init, void *arg, size_t internal_size);

bool DP_output_free(DP_Output *output);

bool DP_output_write(DP_Output *output, const void *buffer, size_t size);

bool DP_output_print(DP_Output *output, const char *string);

bool DP_output_vformat(DP_Output *output, const char *fmt, va_list ap)
    DP_VFORMAT(2);

bool DP_output_format(DP_Output *output, const char *fmt, ...) DP_FORMAT(2, 3);

bool DP_output_clear(DP_Output *output);

bool DP_output_flush(DP_Output *output);

size_t DP_output_tell(DP_Output *output, bool *out_error);

bool DP_output_seek(DP_Output *output, size_t offset);


DP_Output *DP_file_output_new(FILE *fp, bool close);

DP_Output *DP_file_output_new_from_path(const char *path);

// With Qt file IO turned on, this writes to a temporary file and then renames
// it if there were no errors. Otherwise, this just opens the file normally.
// If Qt can't manage to create a temporary file, it will fall back to writing
// the file directly too. This always happens on Android, for example.
DP_Output *DP_file_output_save_new_from_path(const char *path);


DP_Output *DP_gzip_output_new(void *gf, bool close);

DP_Output *DP_gzip_output_new_from_path(const char *path);


DP_Output *DP_mem_output_new(size_t initial_capacity, bool free_on_close,
                             void ***out_buffer, size_t **out_size);


#define DP_OUTPUT_INT8(X)                                       \
    (DP_OutputBinaryEntry)                                      \
    {                                                           \
        .type = DP_OUTPUT_BINARY_TYPE_INT8, .int8 = (int8_t)(X) \
    }
#define DP_OUTPUT_INT16(X)                                         \
    (DP_OutputBinaryEntry)                                         \
    {                                                              \
        .type = DP_OUTPUT_BINARY_TYPE_INT16, .int16 = (int16_t)(X) \
    }
#define DP_OUTPUT_INT32(X)                                         \
    (DP_OutputBinaryEntry)                                         \
    {                                                              \
        .type = DP_OUTPUT_BINARY_TYPE_INT32, .int32 = (int32_t)(X) \
    }
#define DP_OUTPUT_INT64(X)                                         \
    (DP_OutputBinaryEntry)                                         \
    {                                                              \
        .type = DP_OUTPUT_BINARY_TYPE_INT64, .int64 = (int64_t)(X) \
    }
#define DP_OUTPUT_UINT8(X)                                         \
    (DP_OutputBinaryEntry)                                         \
    {                                                              \
        .type = DP_OUTPUT_BINARY_TYPE_UINT8, .uint8 = (uint8_t)(X) \
    }
#define DP_OUTPUT_UINT16(X)                                           \
    (DP_OutputBinaryEntry)                                            \
    {                                                                 \
        .type = DP_OUTPUT_BINARY_TYPE_UINT16, .uint16 = (uint16_t)(X) \
    }
#define DP_OUTPUT_UINT32(X)                                           \
    (DP_OutputBinaryEntry)                                            \
    {                                                                 \
        .type = DP_OUTPUT_BINARY_TYPE_UINT32, .uint32 = (uint32_t)(X) \
    }
#define DP_OUTPUT_UINT64(X)                                           \
    (DP_OutputBinaryEntry)                                            \
    {                                                                 \
        .type = DP_OUTPUT_BINARY_TYPE_UINT64, .uint64 = (uint64_t)(X) \
    }
// Only for small, fixed buffers <= the size of a DP_OutputBinaryEntry!
#define DP_OUTPUT_BYTES(X, COUNT)     \
    (DP_OutputBinaryEntry)            \
    {                                 \
        .type = (COUNT), .bytes = (X) \
    }
#define DP_OUTPUT_END                     \
    (DP_OutputBinaryEntry)                \
    {                                     \
        .type = DP_OUTPUT_BINARY_TYPE_END \
    }

#define DP_OUTPUT_WRITE_BIGENDIAN(OUTPUT, ...) \
    DP_output_write_binary_bigendian(          \
        (OUTPUT), (DP_OutputBinaryEntry[]){__VA_ARGS__, DP_OUTPUT_END})

#define DP_OUTPUT_WRITE_LITTLEENDIAN(OUTPUT, ...) \
    DP_output_write_binary_littleendian(          \
        (OUTPUT), (DP_OutputBinaryEntry[]){__VA_ARGS__, DP_OUTPUT_END})

// Negative numbers so that the type can double as a size for a byte array.
typedef enum DP_OutputBinaryType {
    DP_OUTPUT_BINARY_TYPE_END = 0,
    DP_OUTPUT_BINARY_TYPE_INT8 = -1,
    DP_OUTPUT_BINARY_TYPE_INT16 = -2,
    DP_OUTPUT_BINARY_TYPE_INT32 = -3,
    DP_OUTPUT_BINARY_TYPE_INT64 = -4,
    DP_OUTPUT_BINARY_TYPE_UINT8 = -5,
    DP_OUTPUT_BINARY_TYPE_UINT16 = -6,
    DP_OUTPUT_BINARY_TYPE_UINT32 = -7,
    DP_OUTPUT_BINARY_TYPE_UINT64 = -8,
} DP_OutputBinaryType;

typedef struct DP_OutputBinaryEntry {
    int type;
    union {
        int8_t int8;
        int16_t int16;
        int32_t int32;
        int64_t int64;
        uint8_t uint8;
        uint16_t uint16;
        uint32_t uint32;
        uint64_t uint64;
        const void *bytes;
    };
} DP_OutputBinaryEntry;


bool DP_output_write_binary_bigendian(DP_Output *output,
                                      DP_OutputBinaryEntry *entries);

// Writes little-endian binary data to the given output. The given entries must
// end with an entry of type DP_OUTPUT_END. The contents of the array will be
// clobbered, since its memory is re-used as a write buffer.
bool DP_output_write_binary_littleendian(DP_Output *output,
                                         DP_OutputBinaryEntry *entries);


#endif
