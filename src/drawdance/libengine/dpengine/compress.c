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
#include "compress.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <zlib.h>


static voidpf malloc_z(DP_UNUSED voidpf opaque, uInt items, uInt size)
{
    return DP_malloc((size_t)items * (size_t)size);
}

static void free_z(DP_UNUSED voidpf opaque, voidpf address)
{
    DP_free(address);
}

static const char *get_z_error(z_stream *stream)
{
    const char *msg = stream->msg;
    return msg ? msg : "no error message";
}


static void free_inflate_z_stream(z_stream *stream)
{
    int ret = inflateEnd(stream);
    if (ret != Z_OK) {
        DP_warn("Inflate end error %d: %s", ret, get_z_error(stream));
    }
}

bool DP_compress_inflate(const unsigned char *in, size_t in_size,
                         unsigned char *(*get_output_buffer)(size_t, void *),
                         void *user)
{
    if (in_size < 4) {
        DP_error_set("Inflate input too short to fit header");
        return false;
    }

    size_t out_size = DP_read_bigendian_uint32(in);
    unsigned char *out = get_output_buffer(out_size, user);
    if (!out) {
        return false; // The function should have already set the error message.
    }

    z_stream stream = {0};
    stream.zalloc = malloc_z;
    stream.zfree = free_z;

    int ret = inflateInit(&stream);
    if (ret != Z_OK) {
        DP_error_set("Inflate init error %d: %s", ret, get_z_error(&stream));
        return false;
    }

    stream.avail_out = DP_size_to_uint(out_size);
    stream.next_out = out;
    stream.avail_in = DP_size_to_uint(in_size - 4);
    stream.next_in = (z_const unsigned char *)(in + 4);

    ret = inflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        DP_error_set("Inflate decompression error %d: %s", ret,
                     get_z_error(&stream));
        free_inflate_z_stream(&stream);
        return false;
    }

    unsigned int left = stream.avail_out;
    free_inflate_z_stream(&stream);
    if (left != 0) {
        DP_error_set("Inflate result is %u bytes too short", left);
        return false;
    }

    return true;
}


static void free_deflate_z_stream(z_stream *stream)
{
    int ret = deflateEnd(stream);
    if (ret != Z_OK) {
        DP_warn("Deflate end error %d: %s", ret, get_z_error(stream));
    }
}

size_t DP_compress_deflate_bound(size_t in_size)
{
    unsigned long bound = compressBound(DP_size_to_ulong(in_size));
    return DP_ulong_to_size(bound) + (size_t)4;
}

size_t DP_compress_deflate(const unsigned char *in, size_t in_size,
                           unsigned char *(*get_output_buffer)(size_t, void *),
                           void *user)
{
    z_stream stream = {0};
    stream.zalloc = malloc_z;
    stream.zfree = free_z;

    int ret = deflateInit(&stream, 9);
    if (ret != Z_OK) {
        DP_error_set("Deflate init error %d: %s", ret, get_z_error(&stream));
        return 0;
    }

    unsigned long bound = deflateBound(&stream, DP_size_to_ulong(in_size));
    size_t out_size = bound + 4;

    unsigned char *out = get_output_buffer(out_size, user);
    if (!out) {
        free_deflate_z_stream(&stream);
        return 0; // The function should have already set the error message.
    }

    DP_write_bigendian_uint32(DP_size_to_uint32(in_size), out);

    stream.avail_out = DP_ulong_to_uint(bound);
    stream.next_out = out + 4;
    stream.avail_in = DP_size_to_uint(in_size);
    stream.next_in = (z_const unsigned char *)in;
    ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        DP_error_set("Deflate compression error %d: %s", ret,
                     get_z_error(&stream));
        free_deflate_z_stream(&stream);
        return 0;
    }

    size_t out_used = out_size - stream.avail_out;
    free_deflate_z_stream(&stream);
    return out_used;
}
