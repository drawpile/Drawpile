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

static const char symbols[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

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
        /* fallthrough */
    case 2u:
        buf[bufsize - 2u] = '=';
        /* fallthrough */
    default:
        break;
    }

    buf[bufsize - 1u] = '\0';

    if (out_length) {
        *out_length = bufsize - 1u;
    }
    return buf;
}
