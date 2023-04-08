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
#include "endianness.h"

static_assert(sizeof(int8_t) == 1, "int8_t has size 1");
static_assert(sizeof(uint8_t) == 1, "uint8_t has size 1");
static_assert(sizeof(int16_t) == 2, "int16_t has size 2");
static_assert(sizeof(uint16_t) == 2, "uint16_t has size 2");
static_assert(sizeof(int32_t) == 4, "int32_t has size 4");
static_assert(sizeof(uint32_t) == 4, "uint32_t has size 4");


int8_t DP_read_littleendian_int8(const unsigned char *d)
{
    return DP_uint8_to_int8(DP_read_littleendian_uint8(d));
}

int16_t DP_read_littleendian_int16(const unsigned char *d)
{
    return DP_uint16_to_int16(DP_read_littleendian_uint16(d));
}

int32_t DP_read_littleendian_int32(const unsigned char *d)
{
    return DP_uint32_to_int32(DP_read_littleendian_uint32(d));
}

uint8_t DP_read_littleendian_uint8(const unsigned char *d)
{
    DP_ASSERT(d);
    return d[0];
}

uint16_t DP_read_littleendian_uint16(const unsigned char *d)
{
    DP_ASSERT(d);
    return DP_int_to_uint16(d[0] + (d[1] << 8));
}

uint32_t DP_read_littleendian_uint32(const unsigned char *d)
{
    DP_ASSERT(d);
    return DP_int_to_uint32(d[0] + (d[1] << 8) + (d[2] << 16) + (d[3] << 24));
}

uint64_t DP_read_littleendian_uint64(const unsigned char *d)
{
    DP_ASSERT(d);
    return (DP_uchar_to_uint64(d[0]) << (uint64_t)0)
         + (DP_uchar_to_uint64(d[1]) << (uint64_t)8)
         + (DP_uchar_to_uint64(d[2]) << (uint64_t)16)
         + (DP_uchar_to_uint64(d[3]) << (uint64_t)24)
         + (DP_uchar_to_uint64(d[4]) << (uint64_t)32)
         + (DP_uchar_to_uint64(d[5]) << (uint64_t)40)
         + (DP_uchar_to_uint64(d[6]) << (uint64_t)48)
         + (DP_uchar_to_uint64(d[7]) << (uint64_t)56);
}


int8_t DP_read_bigendian_int8(const unsigned char *d)
{
    return DP_uint8_to_int8(DP_read_bigendian_uint8(d));
}

int16_t DP_read_bigendian_int16(const unsigned char *d)
{
    return DP_uint16_to_int16(DP_read_bigendian_uint16(d));
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
    return DP_int_to_uint16((d[0] << 8) + d[1]);
}

uint32_t DP_read_bigendian_uint32(const unsigned char *d)
{
    DP_ASSERT(d);
    return DP_int_to_uint32((d[0] << 24) + (d[1] << 16) + (d[2] << 8) + d[3]);
}


size_t DP_write_littleendian_int8(int8_t x, unsigned char *out)
{
    return DP_write_littleendian_uint8((uint8_t)x, out);
}

size_t DP_write_littleendian_int16(int16_t x, unsigned char *out)
{
    return DP_write_littleendian_uint16((uint16_t)x, out);
}

size_t DP_write_littleendian_int32(int32_t x, unsigned char *out)
{
    return DP_write_littleendian_uint32((uint32_t)x, out);
}

size_t DP_write_littleendian_int64(int64_t x, unsigned char *out)
{
    return DP_write_littleendian_uint64((uint64_t)x, out);
}

size_t DP_write_littleendian_uint8(uint8_t x, unsigned char *out)
{
    out[0] = x;
    return 1;
}

size_t DP_write_littleendian_uint16(uint16_t x, unsigned char *out)
{
    out[0] = DP_uint_to_uchar(x & 0xffu);
    out[1] = DP_uint_to_uchar((x >> 8u) & 0xffu);
    return 2;
}

size_t DP_write_littleendian_uint32(uint32_t x, unsigned char *out)
{
    out[0] = DP_uint_to_uchar(x & 0xffu);
    out[1] = DP_uint_to_uchar((x >> 8u) & 0xffu);
    out[2] = DP_uint_to_uchar((x >> 16u) & 0xffu);
    out[3] = DP_uint_to_uchar((x >> 24u) & 0xffu);
    return 4;
}

size_t DP_write_littleendian_uint64(uint64_t x, unsigned char *out)
{
    for (int i = 0; i < 8; ++i) {
        out[i] = DP_uint64_to_uchar(x & (uint64_t)0xff);
        x >>= (uint64_t)8;
    }
    return 8;
}


size_t DP_write_bigendian_int8(int8_t x, unsigned char *out)
{
    return DP_write_bigendian_uint8((uint8_t)x, out);
}

size_t DP_write_bigendian_int16(int16_t x, unsigned char *out)
{
    return DP_write_bigendian_uint16((uint16_t)x, out);
}

size_t DP_write_bigendian_int32(int32_t x, unsigned char *out)
{
    return DP_write_bigendian_uint32((uint32_t)x, out);
}

size_t DP_write_bigendian_int64(int64_t x, unsigned char *out)
{
    return DP_write_bigendian_uint64((uint64_t)x, out);
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

size_t DP_write_bigendian_uint64(uint64_t x, unsigned char *out)
{
    for (int i = 0; i < 8; ++i) {
        out[7 - i] = DP_uint64_to_uchar(x & (uint64_t)0xff);
        x >>= (uint64_t)8;
    }
    return 8;
}


size_t DP_write_bytes(const void *DP_RESTRICT x, int count, size_t elem_size,
                      unsigned char *DP_RESTRICT out)
{
    if (count > 0) {
        size_t size = DP_int_to_size(count) * elem_size;
        memcpy(out, x, size);
        return size;
    }
    else {
        return 0;
    }
}

size_t DP_write_bigendian_int8_array(const int8_t *DP_RESTRICT x, int count,
                                     unsigned char *DP_RESTRICT out)
{
    return DP_write_bigendian_uint8_array((const uint8_t *)x, count, out);
}

size_t DP_write_bigendian_int16_array(const int16_t *DP_RESTRICT x, int count,
                                      unsigned char *DP_RESTRICT out)
{
    return DP_write_bigendian_uint16_array((const uint16_t *)x, count, out);
}

size_t DP_write_bigendian_int32_array(const int32_t *DP_RESTRICT x, int count,
                                      unsigned char *DP_RESTRICT out)
{
    return DP_write_bigendian_uint32_array((const uint32_t *)x, count, out);
}

size_t DP_write_bigendian_uint8_array(const uint8_t *DP_RESTRICT x, int count,
                                      unsigned char *DP_RESTRICT out)
{
    return DP_write_bytes(x, count, sizeof(*x), out);
}

size_t DP_write_bigendian_uint16_array(const uint16_t *DP_RESTRICT x, int count,
                                       unsigned char *DP_RESTRICT out)
{
#if defined(DP_BYTE_ORDER_LITTLE_ENDIAN)
    if (count > 0) {
        size_t written = 0;
        for (int i = 0; i < count; ++i) {
            written += DP_write_bigendian_uint16(x[i], out + written);
        }
        return written;
    }
    else {
        return 0;
    }
#elif defined(DP_BYTE_ORDER_BIG_ENDIAN)
    return DP_write_bytes(x, count, sizeof(*x), out);
#else
#    error "Unknown byte order"
#endif
}

size_t DP_write_bigendian_uint32_array(const uint32_t *DP_RESTRICT x, int count,
                                       unsigned char *DP_RESTRICT out)
{
#if defined(DP_BYTE_ORDER_LITTLE_ENDIAN)
    if (count > 0) {
        size_t written = 0;
        for (int i = 0; i < count; ++i) {
            written += DP_write_bigendian_uint32(x[i], out + written);
        }
        return written;
    }
    else {
        return 0;
    }
#elif defined(DP_BYTE_ORDER_BIG_ENDIAN)
    return DP_write_bytes(x, count, sizeof(*x), out);
#else
#    error "Unknown byte order"
#endif
}


uint32_t DP_swap_uint32(uint32_t x)
{
    return ((x & 0x000000ffu) << 24u) | ((x & 0x0000ff00u) << 8u)
         | ((x & 0x00ff0000u) >> 8u) | ((x & 0xff000000u) >> 24u);
}
