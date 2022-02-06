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
#include "binary.h"
#include "common.h"
#include "conversions.h"

static_assert(sizeof(int8_t) == 1, "int8_t has size 1");
static_assert(sizeof(uint8_t) == 1, "uint8_t has size 1");
static_assert(sizeof(int16_t) == 2, "int16_t has size 2");
static_assert(sizeof(uint16_t) == 2, "uint16_t has size 2");
static_assert(sizeof(int32_t) == 4, "int32_t has size 4");
static_assert(sizeof(uint32_t) == 4, "uint32_t has size 4");


int8_t DP_read_bigendian_int8(const unsigned char *d)
{
    return DP_uint8_to_int8(DP_read_bigendian_uint8(d));
}

int32_t DP_read_bigendian_int32(const unsigned char *d)
{
    return DP_uint32_to_int32(DP_read_bigendian_uint32(d));
}

uint8_t DP_read_bigendian_uint8(const unsigned char *d)
{
    DP_ASSERT(d);
    return d[0];
}

uint16_t DP_read_bigendian_uint16(const unsigned char *d)
{
    DP_ASSERT(d);
    return DP_int_to_uint16(d[0] << 8) + DP_uchar_to_uint16(d[1]);
}

uint32_t DP_read_bigendian_uint32(const unsigned char *d)
{
    DP_ASSERT(d);
    return DP_int_to_uint32(d[0] << 24) + DP_int_to_uint32(d[1] << 16)
         + DP_int_to_uint32(d[2] << 8) + DP_uchar_to_uint32(d[3]);
}


size_t DP_write_bigendian_int8(int8_t x, unsigned char *out)
{
    return DP_write_bigendian_uint8((uint8_t)x, out);
}

size_t DP_write_bigendian_int32(int32_t x, unsigned char *out)
{
    return DP_write_bigendian_uint32((uint32_t)x, out);
}

size_t DP_write_bigendian_uint8(uint8_t x, unsigned char *out)
{
    out[0] = x;
    return 1;
}

size_t DP_write_bigendian_uint16(uint16_t x, unsigned char *out)
{
    out[0] = DP_uint_to_uchar((x >> 8u) & 0xffu);
    out[1] = DP_uint_to_uchar(x & 0xffu);
    return 2;
}

size_t DP_write_bigendian_uint32(uint32_t x, unsigned char *out)
{
    out[0] = DP_uint_to_uchar((x >> 24u) & 0xffu);
    out[1] = DP_uint_to_uchar((x >> 16u) & 0xffu);
    out[2] = DP_uint_to_uchar((x >> 8u) & 0xffu);
    out[3] = DP_uint_to_uchar(x & 0xffu);
    return 4;
}


uint32_t DP_swap_uint32(uint32_t x)
{
    return ((x & 0x000000ffu) << 24u) | ((x & 0x0000ff00u) << 8u)
         | ((x & 0x00ff0000u) >> 8u) | ((x & 0xff000000u) >> 24u);
}
