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
#include "base64.h"
#include "common.h"

// Automatic formatting for this X macro is completely bonkers.
// clang-format off
#define BASE64_SYMBOLS_LIST() \
    BASE64_X('A', 0) BASE64_X('B', 1) BASE64_X('C', 2) \
    BASE64_X('D', 3) BASE64_X('E', 4) BASE64_X('F', 5) \
    BASE64_X('G', 6) BASE64_X('H', 7) BASE64_X('I', 8) \
    BASE64_X('J', 9) BASE64_X('K', 10) BASE64_X('L', 11) \
    BASE64_X('M', 12) BASE64_X('N', 13) BASE64_X('O', 14) \
    BASE64_X('P', 15) BASE64_X('Q', 16) BASE64_X('R', 17) \
    BASE64_X('S', 18) BASE64_X('T', 19) BASE64_X('U', 20) \
    BASE64_X('V', 21) BASE64_X('W', 22) BASE64_X('X', 23) \
    BASE64_X('Y', 24) BASE64_X('Z', 25) BASE64_X('a', 26) \
    BASE64_X('b', 27) BASE64_X('c', 28) BASE64_X('d', 29) \
    BASE64_X('e', 30) BASE64_X('f', 31) BASE64_X('g', 32) \
    BASE64_X('h', 33) BASE64_X('i', 34) BASE64_X('j', 35) \
    BASE64_X('k', 36) BASE64_X('l', 37) BASE64_X('m', 38) \
    BASE64_X('n', 39) BASE64_X('o', 40) BASE64_X('p', 41) \
    BASE64_X('q', 42) BASE64_X('r', 43) BASE64_X('s', 44) \
    BASE64_X('t', 45) BASE64_X('u', 46) BASE64_X('v', 47) \
    BASE64_X('w', 48) BASE64_X('x', 49) BASE64_X('y', 50) \
    BASE64_X('z', 51) BASE64_X('0', 52) BASE64_X('1', 53) \
    BASE64_X('2', 54) BASE64_X('3', 55) BASE64_X('4', 56) \
    BASE64_X('5', 57) BASE64_X('6', 58) BASE64_X('7', 59) \
    BASE64_X('8', 60) BASE64_X('9', 61) BASE64_X('+', 62) \
    BASE64_X('/', 63)
// clang-format on

#define BASE64_X(C, N) C,
static const char symbols[] = {BASE64_SYMBOLS_LIST()};
#undef BASE64_X

char *DP_base64_encode(const unsigned char *in, size_t in_length,
                       size_t *out_length)
{
    DP_ASSERT(in || in_length == 0);
    size_t bufsize = 4u * ((in_length + 2u) / 3u) + 1u;
    char *buf = DP_malloc(bufsize);

    for (size_t i = 0u, j = 0u; i < in_length; i += 3u) {
        uint32_t a = in[i];
        uint32_t b = i + 1u < in_length ? in[i + 1u] : 0u;
        uint32_t c = i + 2u < in_length ? in[i + 2u] : 0u;
        uint32_t d = (a << 16u) | (b << 8u) | c;

        buf[j++] = symbols[(d >> 18u) & 0x3fu];
        buf[j++] = symbols[(d >> 12u) & 0x3fu];
        buf[j++] = symbols[(d >> 6u) & 0x3fu];
        buf[j++] = symbols[d & 0x3fu];
    }

    switch (in_length % 3u) {
    case 1u:
        buf[bufsize - 3u] = '=';
        DP_FALLTHROUGH();
    case 2u:
        buf[bufsize - 2u] = '=';
        DP_FALLTHROUGH();
    default:
        break;
    }

    buf[bufsize - 1u] = '\0';

    if (out_length) {
        *out_length = bufsize - 1u;
    }
    return buf;
}

size_t DP_base64_decode_length(const char *in, size_t in_length)
{
    DP_ASSERT(in || in_length == 0);

    size_t len = 0;
    for (size_t i = 0; i < in_length; ++i) {
        char c = in[i];
        switch (c) {
#define BASE64_X(C, N) case C:
            BASE64_SYMBOLS_LIST()
#undef BASE64_X
            ++len;
            break;
        default:
            break;
        }
    }

    switch (len % 4) {
    case 0:
        return len / 4 * 3;
    case 1:
        DP_warn("Invalid base64 input length %zu", len);
        return (len - 1) / 4 * 3;
    case 2:
        return (len + 2) / 4 * 3 - 2;
    case 3:
        return (len + 1) / 4 * 3 - 1;
    default:
        DP_UNREACHABLE();
    }
}

static uint32_t get_next_encoded(const char *in, size_t in_length,
                                 size_t *in_index)
{
    size_t i;
    for (i = *in_index; i < in_length; ++i) {
        char c = in[i];
        switch (c) {
#define BASE64_X(C, N)     \
    case C:                \
        *in_index = i + 1; \
        return N;
            BASE64_SYMBOLS_LIST()
#undef BASE64_X
        default:
            break;
        }
    }
    *in_index = i;
    return 0;
}

void DP_base64_decode(const char *in, size_t in_length, unsigned char *out,
                      size_t out_length)
{
    DP_ASSERT(in || in_length == 0);
    DP_ASSERT(out || in_length == 0);

    size_t in_index = 0;
    size_t out_index = 0;
    while (out_index < out_length) {
        uint32_t a = get_next_encoded(in, in_length, &in_index);
        uint32_t b = get_next_encoded(in, in_length, &in_index);
        uint32_t c = get_next_encoded(in, in_length, &in_index);
        uint32_t d = get_next_encoded(in, in_length, &in_index);
        uint32_t triple = (a << 18u) + (b << 12u) + (c << 6u) + d;

        out[out_index++] = (triple >> 16u) & 0xffu;
        if (out_index < out_length) {
            out[out_index++] = (triple >> 8u) & 0xffu;
            if (out_index < out_length) {
                out[out_index++] = triple & 0xffu;
            }
        }
    }
}
