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
#include <stdint.h>
#include <stdlib.h>

int8_t DP_read_bigendian_int8(const unsigned char *d);
int32_t DP_read_bigendian_int32(const unsigned char *d);
uint8_t DP_read_bigendian_uint8(const unsigned char *d);
uint16_t DP_read_bigendian_uint16(const unsigned char *d);
uint32_t DP_read_bigendian_uint32(const unsigned char *d);

size_t DP_write_bigendian_int8(int8_t x, unsigned char *out);
size_t DP_write_bigendian_int32(int32_t x, unsigned char *out);
size_t DP_write_bigendian_uint8(uint8_t x, unsigned char *out);
size_t DP_write_bigendian_uint16(uint16_t x, unsigned char *out);
size_t DP_write_bigendian_uint32(uint32_t x, unsigned char *out);

uint32_t DP_swap_uint32(uint32_t x);

#endif
