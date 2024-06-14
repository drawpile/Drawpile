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
#include "output.h"
#include "binary.h"
#include "common.h"
#include "conversions.h"
#include <errno.h>
#include <zlib.h>

#ifdef DP_QT_IO
#    include "output_qt.h"
#endif

#define DP_MEM_OUTPUT_MIN_CAPACITY 32


struct DP_Output {
    size_t capacity;
    char *buffer;
    const DP_OutputMethods *methods;
    alignas(DP_max_align_t) unsigned char internal[];
};

DP_Output *DP_output_new(DP_OutputInitFn init, void *arg, size_t internal_size)
{
    DP_ASSERT(init);
    DP_ASSERT(internal_size <= SIZE_MAX - sizeof(DP_Output));
    DP_Output *output = DP_malloc_zeroed(sizeof(*output) + internal_size);
    output->capacity = 0;
    output->buffer = NULL;
    output->methods = init(output->internal, arg);
    if (output->methods) {
        DP_ASSERT(output->methods->write);
        return output;
    }
    else {
        DP_free(output);
        return NULL;
    }
}

static bool free_output(DP_Output *output, bool discard)
{
    if (output) {
        bool (*dispose)(void *, bool) = output->methods->dispose;
        bool ok = dispose ? dispose(output->internal, discard) : true;
        DP_free(output->buffer);
        DP_free(output);
        return ok;
    }
    else {
        return true;
    }
}

bool DP_output_free(DP_Output *output)
{
    return free_output(output, false);
}

bool DP_output_free_discard(DP_Output *output)
{
    return free_output(output, true);
}

bool DP_output_write(DP_Output *output, const void *buffer, size_t size)
{
    if (buffer && size > 0) {
        DP_ASSERT(output);
        return output->methods->write(output->internal, buffer, size) == size;
    }
    else {
        return true;
    }
}

bool DP_output_print(DP_Output *output, const char *string)
{
    return DP_output_write(output, string, string ? strlen(string) : 0);
}

bool DP_output_vformat(DP_Output *output, const char *fmt, va_list ap)
{
    DP_ASSERT(fmt);

    va_list aq;
    va_copy(aq, ap);
    size_t capacity = output->capacity;
    int len = vsnprintf(output->buffer, capacity, fmt, aq);
    va_end(aq);

    if (len > 0) {
        size_t slen = DP_int_to_size(len);
        size_t size = slen + 1;
        if (size > capacity) {
            DP_free(output->buffer);
            output->buffer = DP_malloc(size);
            output->capacity = size;
            if (vsnprintf(output->buffer, size, fmt, ap) != len) {
                DP_error_set("Format length changed");
                return false;
            }
        }
        return DP_output_write(output, output->buffer, slen);
    }
    else if (len == 0) {
        return true;
    }
    else {
        DP_error_set("Format encoding error");
        return false;
    }
}

bool DP_output_format(DP_Output *output, const char *fmt, ...)
{
    DP_ASSERT(output);
    DP_ASSERT(fmt);
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_output_vformat(output, fmt, ap);
    va_end(ap);
    return result;
}

bool DP_output_clear(DP_Output *output)
{
    DP_ASSERT(output);
    bool (*clear)(void *) = output->methods->clear;
    if (clear) {
        return clear(output->internal);
    }
    else {
        DP_error_set("Clearing output not supported");
        return false;
    }
}

bool DP_output_flush(DP_Output *output)
{
    DP_ASSERT(output);
    bool (*flush)(void *) = output->methods->flush;
    return flush ? flush(output->internal) : true;
}

size_t DP_output_tell(DP_Output *output, bool *out_error)
{
    DP_ASSERT(output);
    size_t (*tell)(void *, bool *) = output->methods->tell;
    if (tell) {
        bool error = false;
        size_t result = tell(output->internal, &error);
        if (out_error) {
            *out_error = error;
        }
        return result;
    }
    else {
        DP_error_set("Tell not supported");
        return false;
    }
}

bool DP_output_seek(DP_Output *output, size_t offset)
{
    DP_ASSERT(output);
    bool (*seek)(void *, size_t) = output->methods->seek;
    if (seek) {
        return seek(output->internal, offset);
    }
    else {
        DP_error_set("Seek not supported");
        return false;
    }
}

QIODevice *DP_output_qiodevice(DP_Output *output)
{
    DP_ASSERT(output);
    QIODevice *(*qiodevice)(void *) = output->methods->qiodevice;
    return qiodevice ? qiodevice(output->internal) : NULL;
}


typedef struct DP_FileOutputState {
    FILE *fp;
    bool close;
} DP_FileOutputState;

static size_t file_output_write(void *internal, const void *buffer, size_t size)
{
    DP_FileOutputState *state = internal;
    size_t written = fwrite(buffer, 1, size, state->fp);
    if (written != size) {
        DP_error_set("File output wrote %zu instead of expected %zu bytes: %s",
                     size, written, strerror(errno));
    }
    return written;
}

static bool file_output_flush(void *internal)
{
    DP_FileOutputState *state = internal;
    if (fflush(state->fp) == 0) {
        return true;
    }
    else {
        DP_error_set("File output flush error: %s", strerror(errno));
        return false;
    }
}

static size_t file_output_tell(void *internal, bool *out_error)
{
    DP_FileOutputState *state = internal;
    long offset = ftell(state->fp);
    if (offset != -1) {
        return DP_long_to_size(offset);
    }
    else {
        DP_error_set("File output tell error: %s", strerror(errno));
        *out_error = true;
        return 0;
    }
}

static bool file_output_seek(void *internal, size_t offset)
{
    DP_FileOutputState *state = internal;
    if (fseek(state->fp, DP_size_to_long(offset), SEEK_SET) == 0) {
        return true;
    }
    else {
        DP_error_set("File output could not seek to %zu", offset);
        return false;
    }
}

static bool file_output_dispose(void *internal, bool discard)
{
    DP_FileOutputState *state = internal;
    if (state->close && fclose(state->fp) != 0) {
        if (discard) {
            DP_warn("File output close error: %s", strerror(errno));
        }
        else {
            DP_error_set("File output close error: %s", strerror(errno));
        }
        return false;
    }
    else {
        return true;
    }
}

static const DP_OutputMethods file_output_methods = {
    file_output_write, NULL, file_output_flush,   file_output_tell,
    file_output_seek,  NULL, file_output_dispose,
};

static const DP_OutputMethods *file_output_init(void *internal, void *arg)
{
    *((DP_FileOutputState *)internal) = *((DP_FileOutputState *)arg);
    return &file_output_methods;
}

DP_Output *DP_file_output_new(FILE *fp, bool close)
{
    DP_FileOutputState state = {fp, close};
    return DP_output_new(file_output_init, &state, sizeof(state));
}

DP_Output *DP_file_output_new_from_stdout(bool close)
{
    return DP_file_output_new(stdout, close);
}

DP_Output *DP_file_output_new_from_path(const char *path)
{
    DP_ASSERT(path);
#ifdef DP_QT_IO
    return DP_qfile_output_new_from_path(path, DP_output_new);
#else
    FILE *fp = fopen(path, "wb");
    if (fp) {
        return DP_file_output_new(fp, true);
    }
    else {
        DP_error_set("Can't open '%s': %s", path, strerror(errno));
        return NULL;
    }
#endif
}

DP_Output *DP_file_output_save_new_from_path(const char *path)
{
    DP_ASSERT(path);
#ifdef DP_QT_IO
    return DP_qsavefile_output_new_from_path(path, DP_output_new);
#else
    return DP_file_output_new_from_path(path);
#endif
}


typedef struct DP_GzipOutputState {
    gzFile gf;
    bool close;
} DP_GzipOutputState;

static const char *gzip_output_error(gzFile gf)
{
    int errnum;
    const char *errtext = gzerror(gf, &errnum);
    return errnum == Z_ERRNO ? strerror(errno) : errtext;
}

static size_t gzip_output_write(void *internal, const void *buffer, size_t size)
{
    DP_GzipOutputState *state = internal;
    gzFile gf = state->gf;
#if ZLIB_VERNUM >= 0x1290
    size_t written = gzfwrite(buffer, 1, size, gf);
#else
    int result = gzwrite(gf, buffer, DP_size_to_uint(size));
    size_t written = result <= 0 ? 0 : DP_int_to_size(result);
#endif
    if (written != size) {
        DP_error_set("GZip output wrote %zu instead of expected %zu bytes: %s",
                     size, written, gzip_output_error(gf));
    }
    return written;
}

static bool gzip_output_flush(void *internal)
{
    DP_GzipOutputState *state = internal;
    gzFile gf = state->gf;
    if (gzflush(gf, Z_PARTIAL_FLUSH) == 0) {
        return true;
    }
    else {
        DP_error_set("GZip output flush error: %s", gzip_output_error(gf));
        return false;
    }
}

static size_t gzip_output_tell(void *internal, bool *out_error)
{
    DP_GzipOutputState *state = internal;
    gzFile gf = state->gf;
    z_off_t offset = gztell(gf);
    if (offset != -1) {
        return (size_t)offset;
    }
    else {
        DP_error_set("GZip output tell error: %s", gzip_output_error(gf));
        *out_error = true;
        return 0;
    }
}

static bool gzip_output_seek(void *internal, size_t offset)
{
    DP_GzipOutputState *state = internal;
    gzFile gf = state->gf;
    if (gzseek(gf, (z_off_t)offset, SEEK_SET) == 0) {
        return true;
    }
    else {
        DP_error_set("GZip output could not seek to %zu: %s", offset,
                     gzip_output_error(gf));
        return false;
    }
}

static bool gzip_output_dispose(void *internal, bool discard)
{
    DP_GzipOutputState *state = internal;
    gzFile gf = state->gf;
    if (state->close) {
        int errnum = gzclose(gf);
        if (errnum != Z_OK) {
            if (discard) {
                DP_warn("GZip output close error: %s", zError(errnum));
            }
            else {
                DP_error_set("GZip output close error: %s", zError(errnum));
            }
            return false;
        }
    }
    return true;
}

static const DP_OutputMethods gzip_output_methods = {
    gzip_output_write, NULL, gzip_output_flush,   gzip_output_tell,
    gzip_output_seek,  NULL, gzip_output_dispose,
};

static const DP_OutputMethods *gzip_output_init(void *internal, void *arg)
{
    *((DP_GzipOutputState *)internal) = *((DP_GzipOutputState *)arg);
    return &gzip_output_methods;
}

DP_Output *DP_gzip_output_new(void *gf, bool close)
{
    DP_ASSERT(gf);
    DP_GzipOutputState state = {gf, close};
    return DP_output_new(gzip_output_init, &state, sizeof(state));
}

DP_Output *DP_gzip_output_new_from_path(const char *path)
{
    DP_ASSERT(path);
#if defined(DP_QT_IO) && defined(DP_QT_IO_KARCHIVE)
    return DP_karchive_gzip_output_new_from_path(path, DP_output_new);
#else
    gzFile gf = gzopen(path, "wb");
    if (gf) {
        return DP_gzip_output_new(gf, true);
    }
    else {
        DP_error_set("Can't open '%s': %s", path, strerror(errno));
        return NULL;
    }
#endif
}


typedef struct DP_MemOutputState {
    unsigned char *buffer;
    size_t capacity, used;
    bool free_on_close;
} DP_MemOutputState;

static size_t mem_output_space(DP_MemOutputState *state)
{
    return state->capacity - state->used - 1u;
}

static size_t mem_output_write(void *internal, const void *buffer, size_t size)
{
    DP_MemOutputState *state = internal;
    if (mem_output_space(state) < size) {
        do {
            state->capacity *= 2u;
        } while (mem_output_space(state) < size);
        state->buffer = DP_realloc(state->buffer, state->capacity);
    }
    memcpy(state->buffer + state->used, buffer, size);
    size_t end = state->used + size;
    state->buffer[end] = '\0';
    state->used = end;
    return size;
}

static bool mem_output_clear(void *internal)
{
    DP_MemOutputState *state = internal;
    state->used = 0;
    state->buffer[0] = '\0';
    return true;
}

static bool mem_output_dispose(void *internal, DP_UNUSED bool discard)
{
    DP_MemOutputState *state = internal;
    if (state->free_on_close) {
        free(state->buffer);
    }
    return true;
}

static const DP_OutputMethods mem_output_methods = {
    mem_output_write,   mem_output_clear, NULL, NULL, NULL, NULL,
    mem_output_dispose,
};

static const DP_OutputMethods *mem_output_init(void *internal, void *arg)
{
    *((DP_MemOutputState *)internal) = *((DP_MemOutputState *)arg);
    return &mem_output_methods;
}

static void mem_output_assign_out_vars(DP_Output *output, void ***out_buffer,
                                       size_t **out_size)
{
    DP_MemOutputState *state = (void *)output->internal;
    if (out_buffer) {
        *out_buffer = (void *)&state->buffer;
    }
    if (out_size) {
        *out_size = &state->used;
    }
}

DP_Output *DP_mem_output_new(size_t initial_capacity, bool free_on_close,
                             void ***out_buffer, size_t **out_size)
{
    size_t capacity = initial_capacity < DP_MEM_OUTPUT_MIN_CAPACITY
                        ? DP_MEM_OUTPUT_MIN_CAPACITY
                        : initial_capacity;
    DP_MemOutputState state = {DP_malloc(capacity), capacity, 0, free_on_close};
    state.buffer[0] = '\0';
    DP_Output *output = DP_output_new(mem_output_init, &state, sizeof(state));
    mem_output_assign_out_vars(output, out_buffer, out_size);
    return output;
}


static bool write_binary(DP_Output *output, DP_OutputBinaryEntry *entries,
                         size_t (*write_int8)(int8_t, unsigned char *),
                         size_t (*write_int16)(int16_t, unsigned char *),
                         size_t (*write_int32)(int32_t, unsigned char *),
                         size_t (*write_int64)(int64_t, unsigned char *),
                         size_t (*write_uint8)(uint8_t, unsigned char *),
                         size_t (*write_uint16)(uint16_t, unsigned char *),
                         size_t (*write_uint32)(uint32_t, unsigned char *),
                         size_t (*write_uint64)(uint64_t, unsigned char *))
{
    // We'll re-use the memory of the given entries as a write buffer. This is
    // totally an evil hack, but it's legal and avoids malloc or alloca. There's
    // no issues with strict aliasing since char is allowed to alias anything.
    unsigned char *out = (unsigned char *)entries;
    size_t written = 0;
    for (int i = 0;; ++i) {
        DP_OutputBinaryEntry e = entries[i];
        switch (e.type) {
        case DP_OUTPUT_BINARY_TYPE_END:
            return DP_output_write(output, entries, written);
        case DP_OUTPUT_BINARY_TYPE_INT8:
            written += write_int8(e.int8, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_INT16:
            written += write_int16(e.int16, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_INT32:
            written += write_int32(e.int32, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_INT64:
            written += write_int64(e.int64, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_UINT8:
            written += write_uint8(e.uint8, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_UINT16:
            written += write_uint16(e.uint16, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_UINT32:
            written += write_uint32(e.uint32, out + written);
            break;
        case DP_OUTPUT_BINARY_TYPE_UINT64:
            written += write_uint64(e.uint64, out + written);
            break;
        default:
            DP_ASSERT(e.type > 0);
            DP_ASSERT(DP_int_to_size(e.type) < sizeof(DP_OutputBinaryEntry));
            written += DP_write_bytes(e.bytes, e.type, 1, out + written);
            break;
        }
    }
}

bool DP_output_write_binary_bigendian(DP_Output *output,
                                      DP_OutputBinaryEntry *entries)
{
    DP_ASSERT(output);
    DP_ASSERT(entries);
    return write_binary(output, entries, DP_write_bigendian_int8,
                        DP_write_bigendian_int16, DP_write_bigendian_int32,
                        DP_write_bigendian_int64, DP_write_bigendian_uint8,
                        DP_write_bigendian_uint16, DP_write_bigendian_uint32,
                        DP_write_bigendian_uint64);
}

bool DP_output_write_binary_littleendian(DP_Output *output,
                                         DP_OutputBinaryEntry *entries)
{
    DP_ASSERT(output);
    DP_ASSERT(entries);
    return write_binary(
        output, entries, DP_write_littleendian_int8,
        DP_write_littleendian_int16, DP_write_littleendian_int32,
        DP_write_littleendian_int64, DP_write_littleendian_uint8,
        DP_write_littleendian_uint16, DP_write_littleendian_uint32,
        DP_write_littleendian_uint64);
}
