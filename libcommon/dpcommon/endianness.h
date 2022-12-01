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
#ifndef DPCOMMON_ENDIANNESS_H
#define DPCOMMON_ENDIANNESS_H


#define DP_LITTLE_ENDIAN 1
#define DP_BIG_ENDIAN    2

#ifndef DP_BYTE_ORDER
#    ifdef __BYTE_ORDER__
#        if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#            define DP_BYTE_ORDER DP_LITTLE_ENDIAN
#        elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#            define DP_BYTE_ORDER DP_BIG_ENDIAN
#        else
#            error "Unknown __BYTE_ORDER__, #define DP_BYTE_ORDER manually"
#        endif
#    else
#        error "Can't figure out byte order, #define DP_BYTE_ORDER manually"
#    endif
#endif


#undef DP_BYTE_ORDER_LITTLE_ENDIAN
#undef DP_BYTE_ORDER_BIG_ENDIAN

#if DP_BYTE_ORDER == DP_LITTLE_ENDIAN
#    define DP_BYTE_ORDER_LITTLE_ENDIAN DP_LITTLE_ENDIAN
#elif DP_BYTE_ORDER == DP_BIG_ENDIAN
#    define DP_BYTE_ORDER_BIG_ENDIAN DP_BIG_ENDIAN
#else
#    error "DP_BYTE_ORDER defined to an unknown value"
#endif


#endif
