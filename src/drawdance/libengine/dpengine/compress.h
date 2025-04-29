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
#ifndef DPENGINE_COMPRESS_H
#define DPENGINE_COMPRESS_H
#include <dpcommon/common.h>

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;


bool DP_decompress_deflate(const unsigned char *in, size_t in_size,
                           unsigned char *(*get_output_buffer)(size_t, void *),
                           void *user);

size_t DP_compress_deflate_bound(size_t in_size);

size_t DP_compress_deflate(const unsigned char *in, size_t in_size,
                           unsigned char *(*get_output_buffer)(size_t, void *),
                           void *user);


bool DP_decompress_zstd(ZSTD_DCtx **in_out_ctx_or_null, const unsigned char *in,
                        size_t in_size,
                        unsigned char *(*get_output_buffer)(size_t, void *),
                        void *user);

void DP_decompress_zstd_free(ZSTD_DCtx **in_out_ctx_or_null);


size_t DP_compress_zstd(ZSTD_CCtx **in_out_ctx_or_null, const unsigned char *in,
                        size_t in_size,
                        unsigned char *(*get_output_buffer)(size_t, void *),
                        void *user);

size_t DP_compress_zstd_bounds(size_t in_size);

void DP_compress_zstd_free(ZSTD_CCtx **in_out_ctx_or_null);


#endif
