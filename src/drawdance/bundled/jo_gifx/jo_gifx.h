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
 *
 * --------------------------------------------------------------------
 *
 * This is a heavily modified version of jo_gif, a simple, minimalistic GIF
 * writer by Jon Olick, placed into the public domain: http://jonolick.com
 *
 * It has been modified to support big endian machines, bgra color instead of
 * argb, output-agnostic functions and error checking. It also doesn't use large
 * stack structures and doesn't allocate and free memory for each frame, instead
 * it does a single allocation to hold everything.
 */
#ifndef JO_GIFX_H
#define JO_GIFX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct jo_gifx_t jo_gifx_t;
typedef bool (*jo_gifx_write_fn)(void *user, const void *buffer, size_t size);

// Allocate handle, write header. Width and height are the image dimensions,
// repeat is animation repetition count (0 means forever, anything that doesn't
// fit into a uint16_t means no animation), numColors is the number of colors in
// the palette, must be a power of 2 - 1 (e.g. 255). The given rgba pixels will
// be used to generate a palette. Returns either the handle or NULL if writing
// the header failed.
jo_gifx_t *jo_gifx_start(jo_gifx_write_fn write_fn, void *user, uint16_t width,
                         uint16_t height, int repeat, int numColors,
                         uint32_t *rgba);

// Appends a frame with the given pixels, delayCsec is the amount of time the
// frame persists in centiseconds (hundredth of a second, not milliseconds.)
bool jo_gifx_frame(jo_gifx_write_fn write_fn, void *user, jo_gifx_t *gif,
                   uint32_t *rgba, uint16_t delayCsec);

// Frees the handle, writes trailer and returns if that worked. The handle
// *always* gets freed, even if writing the trailer failed.
bool jo_gifx_end(jo_gifx_write_fn write_fn, void *user, jo_gifx_t *gif);

// Frees the handle without writing a trailer.
void jo_gifx_abort(jo_gifx_t *gif);

#endif
