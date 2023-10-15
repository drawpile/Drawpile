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
#include "file.h"
#include "common.h"
#include "conversions.h"
#include "input.h"
#include "output.h"
#include <ctype.h>
#include <string.h>
#ifdef _WIN32
#    include <io.h>
#else
#    include <unistd.h>
#endif


void *DP_file_slurp(const char *path, size_t *out_length)
{
    DP_ASSERT(path);
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        return NULL;
    }

    bool error;
    size_t length = DP_input_length(input, &error);
    if (error) {
        DP_input_free(input);
        return NULL;
    }

    size_t size = length + 1;
    char *buf = DP_malloc(size);
    size_t read = DP_input_read(input, buf, length, &error);
    if (error) {
        DP_free(buf);
        DP_input_free(input);
        return NULL;
    }
    else if (read != length) {
        DP_error_set("Can't read %zu bytes from '%s': got %zu bytes", length,
                     path, read);
        DP_free(buf);
        DP_input_free(input);
        return NULL;
    }

    buf[length] = '\0';
    if (out_length) {
        *out_length = length;
    }

    DP_input_free(input);
    return buf;
}


bool DP_file_exists(const char *path)
{
    DP_ASSERT(path);
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}


bool DP_file_copy(const char *source_path, const char *target_path)
{
    DP_Input *input = DP_file_input_new_from_path(source_path);
    if (!input) {
        return false;
    }

    DP_Output *output = DP_file_output_new_from_path(target_path);
    if (!output) {
        DP_input_free(input);
        return false;
    }

    bool error = false;
    unsigned char *buffer = DP_malloc(BUFSIZ);
    while (true) {
        size_t read = DP_input_read(input, buffer, BUFSIZ, &error);
        if (error || read == 0) {
            break;
        }
        else if (!DP_output_write(output, buffer, read)) {
            error = true;
            break;
        }
    }
    DP_free(buffer);

    if (!DP_output_free(output)) {
        error = true;
    }
    DP_input_free(input);
    return !error;
}


static int find_prefix_len(const char *start, const char *end,
                           int *out_last_index)
{
    // If there's already a " (1)" or something before the end, chop that off.
    const char *open = NULL;
    const char *close = NULL;
    bool all_digits = true;
    for (const char *p = start; p < end; ++p) {
        char c = *p;
        if (c == '(') {
            open = p;
            close = NULL;
            all_digits = true;
        }
        else if (c == ')') {
            close = p;
        }
        else if (open && !close && !isdigit(c)) {
            all_digits = false;
        }
    }

    if (open && close && open > start && *(open - 1) == ' ' && close + 1 == end
        && open + 1 < close && all_digits) {
        *out_last_index = atoi(open + 1);
        return (int)(open - start - 1);
    }
    else {
        return (int)(end - start);
    }
}

char *DP_file_path_unique(const char *path, int max,
                          DP_FilePathUniqueAcceptFn accept, void *user)
{
    DP_ASSERT(path);
    size_t len = strlen(path);
    const char *dot = strrchr(path, '.');
    // If the dot is absent or at the beginning, we don't have an extension.
    bool ext_present = dot && dot != path;
    int last_index = -1;
    int prefix_len =
        find_prefix_len(path, ext_present ? dot : path + len, &last_index);
    int next_index = last_index + 1;
    if (next_index >= max) {
        return NULL;
    }

    void **buffer_ptr;
    DP_Output *output = DP_mem_output_new(len + 8, false, &buffer_ptr, NULL);
    const char *ext = ext_present ? dot : "";

    for (int i = next_index; i <= max; ++i) {
        if (i == 0) {
            DP_output_format(output, "%.*s%s", prefix_len, path, ext);
        }
        else {
            DP_output_format(output, "%.*s (%d)%s", prefix_len, path, i, ext);
        }

        char *buffer = *buffer_ptr;
        if (accept(user, buffer)) {
            DP_output_free(output);
            return buffer;
        }
        else {
            DP_output_clear(output);
        }
    }

    char *buffer = *buffer_ptr;
    DP_output_free(output);
    DP_free(buffer);
    return NULL;
}


static bool accept_nonexistent(DP_UNUSED void *user, const char *path)
{
    return !DP_file_exists(path);
}

char *DP_file_path_unique_nonexistent(const char *path, int max)
{
    return DP_file_path_unique(path, max, accept_nonexistent, NULL);
}
