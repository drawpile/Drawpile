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
    void (*dispose)(void *internal);
} DP_OutputMethods;

typedef const DP_OutputMethods *(*DP_OutputInitFn)(void *internal, void *arg);

DP_Output *DP_output_new(DP_OutputInitFn init, void *arg, size_t internal_size);

void DP_output_free(DP_Output *output);

bool DP_output_write(DP_Output *output, const void *buffer, size_t size);

bool DP_output_print(DP_Output *output, const char *string);

bool DP_output_vformat(DP_Output *output, const char *fmt, va_list ap);

bool DP_output_format(DP_Output *output, const char *fmt, ...) DP_FORMAT(2, 3);

bool DP_output_clear(DP_Output *output);

bool DP_output_flush(DP_Output *output);

size_t DP_output_tell(DP_Output *output, bool *out_error);

bool DP_output_seek(DP_Output *output, size_t offset);


DP_Output *DP_file_output_new(FILE *fp, bool close);

DP_Output *DP_file_output_new_from_path(const char *path);


DP_Output *DP_mem_output_new(size_t initial_capacity, bool free_on_close,
                             void ***out_buffer, size_t **out_size);


#endif
