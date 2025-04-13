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
#ifndef DPCOMMON_BINARY_H
#define DPCOMMON_BINARY_H
#include "common.h"

int8_t DP_read_littleendian_int8(const unsigned char *d);
int16_t DP_read_littleendian_int16(const unsigned char *d);
int32_t DP_read_littleendian_int24(const unsigned char *d);
int32_t DP_read_littleendian_int32(const unsigned char *d);
uint8_t DP_read_littleendian_uint8(const unsigned char *d);
uint16_t DP_read_littleendian_uint16(const unsigned char *d);
uint32_t DP_read_littleendian_uint24(const unsigned char *d);
uint32_t DP_read_littleendian_uint32(const unsigned char *d);
uint64_t DP_read_littleendian_uint64(const unsigned char *d);

int8_t DP_read_bigendian_int8(const unsigned char *d);
int16_t DP_read_bigendian_int16(const unsigned char *d);
int32_t DP_read_bigendian_int24(const unsigned char *d);
int32_t DP_read_bigendian_int32(const unsigned char *d);
uint8_t DP_read_bigendian_uint8(const unsigned char *d);
uint16_t DP_read_bigendian_uint16(const unsigned char *d);
uint32_t DP_read_bigendian_uint24(const unsigned char *d);
uint32_t DP_read_bigendian_uint32(const unsigned char *d);

size_t DP_write_littleendian_int8(int8_t x, unsigned char *out);
size_t DP_write_littleendian_int16(int16_t x, unsigned char *out);
size_t DP_write_littleendian_int24(int32_t x, unsigned char *out);
size_t DP_write_littleendian_int32(int32_t x, unsigned char *out);
size_t DP_write_littleendian_int64(int64_t x, unsigned char *out);
size_t DP_write_littleendian_uint8(uint8_t x, unsigned char *out);
size_t DP_write_littleendian_uint16(uint16_t x, unsigned char *out);
size_t DP_write_littleendian_uint24(uint32_t x, unsigned char *out);
size_t DP_write_littleendian_uint32(uint32_t x, unsigned char *out);
size_t DP_write_littleendian_uint64(uint64_t x, unsigned char *out);

size_t DP_write_bigendian_int8(int8_t x, unsigned char *out);
size_t DP_write_bigendian_int16(int16_t x, unsigned char *out);
size_t DP_write_bigendian_int24(int32_t x, unsigned char *out);
size_t DP_write_bigendian_int32(int32_t x, unsigned char *out);
size_t DP_write_bigendian_int64(int64_t x, unsigned char *out);
size_t DP_write_bigendian_uint8(uint8_t x, unsigned char *out);
size_t DP_write_bigendian_uint16(uint16_t x, unsigned char *out);
size_t DP_write_bigendian_uint24(uint32_t x, unsigned char *out);
size_t DP_write_bigendian_uint32(uint32_t x, unsigned char *out);
size_t DP_write_bigendian_uint64(uint64_t x, unsigned char *out);

size_t DP_write_bytes(const void *DP_RESTRICT x, int count, size_t elem_size,
                      unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_int8_array(const int8_t *DP_RESTRICT x, int count,
                                     unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_int16_array(const int16_t *DP_RESTRICT x, int count,
                                      unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_int32_array(const int32_t *DP_RESTRICT x, int count,
                                      unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_uint8_array(const uint8_t *DP_RESTRICT x, int count,
                                      unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_uint16_array(const uint16_t *DP_RESTRICT x, int count,
                                       unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_uint24_array(const uint32_t *DP_RESTRICT x, int count,
                                       unsigned char *DP_RESTRICT out);

size_t DP_write_bigendian_uint32_array(const uint32_t *DP_RESTRICT x, int count,
                                       unsigned char *DP_RESTRICT out);

uint32_t DP_swap_uint32(uint32_t x);

#endif
