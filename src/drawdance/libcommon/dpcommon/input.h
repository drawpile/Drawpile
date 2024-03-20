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
#ifndef DPCOMMON_INPUT_H
#define DPCOMMON_INPUT_H
#include "common.h"
#include <stdio.h>

#ifdef __cplusplus
class QIODevice;
#else
typedef struct QIODevice QIODevice;
#endif


typedef struct DP_Input DP_Input;

typedef struct DP_InputMethods {
    size_t (*read)(void *internal, void *buffer, size_t size, bool *out_errror);
    size_t (*length)(void *internal, bool *out_error);
    bool (*rewind)(void *internal);
    bool (*rewind_by)(void *internal, size_t size);
    bool (*seek)(void *internal, size_t offset);
    bool (*seek_by)(void *internal, size_t size);
    QIODevice *(*qiodevice)(void *internal);
    void (*dispose)(void *internal);
} DP_InputMethods;

typedef const DP_InputMethods *(*DP_InputInitFn)(void *internal, void *arg);

DP_Input *DP_input_new(DP_InputInitFn init, void *arg, size_t internal_size);

void DP_input_free(DP_Input *input);

size_t DP_input_read(DP_Input *input, void *buffer, size_t size,
                     bool *out_error);

size_t DP_input_length(DP_Input *input, bool *out_error);

bool DP_input_rewind(DP_Input *input);

bool DP_input_rewind_by(DP_Input *input, size_t size);

bool DP_input_seek(DP_Input *input, size_t offset);

bool DP_input_seek_by(DP_Input *input, size_t size);

QIODevice *DP_input_qiodevice(DP_Input *input);


#ifndef RUST_BINDGEN
DP_Input *DP_file_input_new(FILE *fp, bool close);
#endif

DP_Input *DP_file_input_new_from_stdin(bool close);

DP_Input *DP_file_input_new_from_path(const char *path);


typedef void (*DP_MemInputFreeFn)(void *buffer, size_t size, void *free_arg);

DP_Input *DP_mem_input_new(void *buffer, size_t size, DP_MemInputFreeFn free,
                           void *free_arg);

DP_Input *DP_mem_input_new_free_on_close(void *buffer, size_t size);

DP_Input *DP_mem_input_new_keep_on_close(const void *buffer, size_t size);


#define DP_BUFFERED_INPUT_NULL \
    (DP_BufferedInput)        \
    {                         \
        NULL, NULL, 0         \
    }

typedef struct DP_BufferedInput {
    DP_Input *inner;
    unsigned char *buffer;
    size_t capacity;
} DP_BufferedInput;

DP_BufferedInput DP_buffered_input_init(DP_Input *input);

void DP_buffered_input_dispose(DP_BufferedInput *bi);

size_t DP_buffered_input_read(DP_BufferedInput *bi, size_t size,
                              bool *out_error);

bool DP_buffered_input_seek(DP_BufferedInput *bi, size_t offset);


#endif
