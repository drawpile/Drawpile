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
 * argb, output-agnostic functions, error checking, quantizing the palette
 * colors separately instead of using the first frame and cumulative frames
 * with identical pixels between them getting turned transparent. It also
 * doesn't use large stack structures and doesn't allocate and free memory for
 * each frame, instead it does a single allocation to hold everything.
 */
#include "jo_gifx.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define HASH_SIZE 5003

struct jo_gifx_t {
    unsigned char palette[0x300];
    uint16_t width, height;
    int repeat;
    int numColors, palSize;
    int frame;
    short codetab[HASH_SIZE];
    int hashTbl[HASH_SIZE];
    unsigned char pixels[];
};

// Based on NeuQuant algorithm
static void jo_gifx_quantize(uint32_t *rgba, int rgbaSize, int sample,
                             unsigned char *map, int numColors)
{
    // defs for freq and bias
    const int intbiasshift = 16; /* bias for fractions */
    const int intbias = (((int)1) << intbiasshift);
    const int gammashift = 10; /* gamma = 1024 */
    const int betashift = 10;
    const int beta = (intbias >> betashift); /* beta = 1/1024 */
    const int betagamma = (intbias << (gammashift - betashift));

    // defs for decreasing radius factor
    const int radiusbiasshift = 6; /* at 32.0 biased by 6 bits */
    const int radiusbias = (((int)1) << radiusbiasshift);
    const int radiusdec = 30; /* factor of 1/30 each cycle */

    // defs for decreasing alpha factor
    const int alphabiasshift = 10; /* alpha starts at 1.0 */
    const int initalpha = (((int)1) << alphabiasshift);

    // radbias and alpharadbias used for radpower calculation
    const int radbiasshift = 8;
    const int radbias = (((int)1) << radbiasshift);
    const int alpharadbshift = (alphabiasshift + radbiasshift);
    const int alpharadbias = (((int)1) << alpharadbshift);

    sample = sample < 1 ? 1 : sample > 30 ? 30 : sample;
    int network[256][3];
    int bias[256] = {0}, freq[256];
    for (int i = 0; i < numColors; ++i) {
        // Put nurons evenly through the luminance spectrum.
        network[i][0] = network[i][1] = network[i][2] = (i << 12) / numColors;
        freq[i] = intbias / numColors;
    }
    // Learn
    {
        const int primes[5] = {499, 491, 487, 503};
        int step = 4;
        for (int i = 0; i < 4; ++i) {
            if (rgbaSize > primes[i] * 4
                && (rgbaSize % primes[i])) { // TODO/Error? primes[i]*4?
                step = primes[i] * 4;
            }
        }
        sample = step == 4 ? 1 : sample;

        int alphadec = 30 + ((sample - 1) / 3);
        int samplepixels = rgbaSize / (4 * sample);
        int delta = samplepixels / 100;
        int alpha = initalpha;
        delta = delta == 0 ? 1 : delta;

        int radius = (numColors >> 3) * radiusbias;
        int rad = radius >> radiusbiasshift;
        rad = rad <= 1 ? 0 : rad;
        int radSq = rad * rad;
        int radpower[32];
        for (int i = 0; i < rad; i++) {
            radpower[i] = alpha * (((radSq - i * i) * radbias) / radSq);
        }

        // Randomly walk through the pixels and relax neurons to the "optimal"
        // target.
        for (int i = 0, pix = 0; i < samplepixels;) {
            uint32_t color = rgba[pix / 4];
            int r = ((color >> 16) & 0xff) << 4;
            int g = ((color >> 8) & 0xff) << 4;
            int b = (color & 0xff) << 4;
            int j = -1;
            {
                // finds closest neuron (min dist) and updates freq
                // finds best neuron (min dist-bias) and returns position
                // for frequently chosen neurons, freq[k] is high and bias[k] is
                // negative bias[k] = gamma*((1/numColors)-freq[k])

                int bestd = 0x7FFFFFFF, bestbiasd = 0x7FFFFFFF, bestpos = -1;
                for (int k = 0; k < numColors; k++) {
                    int *n = network[k];
                    int dist = abs(n[0] - r) + abs(n[1] - g) + abs(n[2] - b);
                    if (dist < bestd) {
                        bestd = dist;
                        bestpos = k;
                    }
                    int biasdist = dist - ((bias[k]) >> (intbiasshift - 4));
                    if (biasdist < bestbiasd) {
                        bestbiasd = biasdist;
                        j = k;
                    }
                    int betafreq = freq[k] >> betashift;
                    freq[k] -= betafreq;
                    bias[k] += betafreq << gammashift;
                }
                freq[bestpos] += beta;
                bias[bestpos] -= betagamma;
            }

            // Move neuron j towards biased (b,g,r) by factor alpha
            network[j][0] -= (network[j][0] - r) * alpha / initalpha;
            network[j][1] -= (network[j][1] - g) * alpha / initalpha;
            network[j][2] -= (network[j][2] - b) * alpha / initalpha;
            if (rad != 0) {
                // Move adjacent neurons by precomputed
                // alpha*(1-((i-j)^2/[r]^2)) in radpower[|i-j|]
                int lo = j - rad;
                lo = lo < -1 ? -1 : lo;
                int hi = j + rad;
                hi = hi > numColors ? numColors : hi;
                for (int jj = j + 1, m = 1; jj < hi; ++jj) {
                    int a = radpower[m++];
                    network[jj][0] -= (network[jj][0] - r) * a / alpharadbias;
                    network[jj][1] -= (network[jj][1] - g) * a / alpharadbias;
                    network[jj][2] -= (network[jj][2] - b) * a / alpharadbias;
                }
                for (int k = j - 1, m = 1; k > lo; --k) {
                    int a = radpower[m++];
                    network[k][0] -= (network[k][0] - r) * a / alpharadbias;
                    network[k][1] -= (network[k][1] - g) * a / alpharadbias;
                    network[k][2] -= (network[k][2] - b) * a / alpharadbias;
                }
            }

            pix += step;
            pix = pix >= rgbaSize ? pix - rgbaSize : pix;

            // every 1% of the image, move less over the following iterations.
            if (++i % delta == 0) {
                alpha -= alpha / alphadec;
                radius -= radius / radiusdec;
                rad = radius >> radiusbiasshift;
                rad = rad <= 1 ? 0 : rad;
                radSq = rad * rad;
                for (j = 0; j < rad; j++) {
                    radpower[j] = alpha * ((radSq - j * j) * radbias / radSq);
                }
            }
        }
    }
    // Unbias network to give byte values 0..255
    for (int i = 0; i < numColors; i++) {
        map[i * 3 + 0] = network[i][0] >>= 4;
        map[i * 3 + 1] = network[i][1] >>= 4;
        map[i * 3 + 2] = network[i][2] >>= 4;
    }
}

typedef struct {
    int numBits;
    unsigned char buf[256];
    unsigned char idx;
    unsigned tmp;
    int outBits;
    int curBits;
} jo_gifx_lzw_t;

static bool jo_gifx_lzw_write_buf(jo_gifx_write_fn write_fn, void *user,
                                  jo_gifx_lzw_t *s)
{
    return write_fn(user, (unsigned char[]){s->idx}, 1)
        && write_fn(user, s->buf, s->idx);
}

static bool jo_gifx_lzw_write(jo_gifx_write_fn write_fn, void *user,
                              jo_gifx_lzw_t *s, int code)
{
    s->outBits |= code << s->curBits;
    s->curBits += s->numBits;
    while (s->curBits >= 8) {
        s->buf[s->idx++] = s->outBits & 255;
        s->outBits >>= 8;
        s->curBits -= 8;
        if (s->idx >= 255) {
            if (!jo_gifx_lzw_write_buf(write_fn, user, s)) {
                return false;
            }
            s->idx = 0;
        }
    }
    return true;
}

static bool jo_gifx_lzw_encode(jo_gifx_write_fn write_fn, void *user,
                               jo_gifx_t *gif, unsigned char *in, int len)
{
    jo_gifx_lzw_t state = {9};
    int maxcode = 511;

    short *codetab = gif->codetab;
    int *hashTbl = gif->hashTbl;
    memset(hashTbl, 0xFF, sizeof(gif->hashTbl));

    if (!jo_gifx_lzw_write(write_fn, user, &state, 0x100)) {
        return false;
    }

    int free_ent = 0x102;
    int ent = *in++;
CONTINUE:
    while (--len) {
        int c = *in++;
        int fcode = (c << 12) + ent;
        int key = (c << 4) ^ ent; // xor hashing
        while (hashTbl[key] >= 0) {
            if (hashTbl[key] == fcode) {
                ent = codetab[key];
                goto CONTINUE;
            }
            ++key;
            key = key >= HASH_SIZE ? key - HASH_SIZE : key;
        }
        if (!jo_gifx_lzw_write(write_fn, user, &state, ent)) {
            return false;
        }
        ent = c;
        if (free_ent < 4096) {
            if (free_ent > maxcode) {
                ++state.numBits;
                if (state.numBits == 12) {
                    maxcode = 4096;
                }
                else {
                    maxcode = (1 << state.numBits) - 1;
                }
            }
            codetab[key] = free_ent++;
            hashTbl[key] = fcode;
        }
        else {
            memset(hashTbl, 0xFF, sizeof(gif->hashTbl));
            free_ent = 0x102;
            if (!jo_gifx_lzw_write(write_fn, user, &state, 0x100)) {
                return false;
            }
            state.numBits = 9;
            maxcode = 511;
        }
    }

    bool ok = jo_gifx_lzw_write(write_fn, user, &state, ent)
           && jo_gifx_lzw_write(write_fn, user, &state, 0x101)
           && jo_gifx_lzw_write(write_fn, user, &state, 0);
    if (!ok) {
        return false;
    }

    if (state.idx) {
        if (!jo_gifx_lzw_write_buf(write_fn, user, &state)) {
            return false;
        }
    }
    return true;
}

static int jo_gifx_clamp(int a, int b, int c)
{
    return a < b ? b : a > c ? c : a;
}

static bool jo_gifx_write_uint16(jo_gifx_write_fn write_fn, void *user,
                                 uint16_t x)
{
    unsigned char out[] = {x & 0xff, (x >> 8) & 0xff};
    return write_fn(user, out, 2);
}

jo_gifx_t *jo_gifx_start(jo_gifx_write_fn write_fn, void *user, uint16_t width,
                         uint16_t height, int repeat, int numColors)
{
    int palSize = log2(numColors);
    unsigned char header[] = {// "GIF89a" in ASCII
                              0x47, 0x49, 0x46, 0x38, 0x39, 0x61,
                              // Width in 16 bit little-endian
                              width & 0xff, (width >> 8) & 0xff,
                              // Height in 16 bit little-endian
                              height & 0xff, (height >> 8) & 0xff,
                              // Palette size, background color, aspect ratio
                              0xf0 | palSize, 0, 0};
    if (!write_fn(user, header, sizeof(header))) {
        return NULL;
    }

    numColors = numColors > 255 ? 255 : numColors < 2 ? 2 : numColors;
    size_t size = width * height;
    jo_gifx_t *gif = calloc(1, sizeof(*gif) + size * 6);
    if (!gif) {
        return NULL;
    }

    gif->width = width;
    gif->height = height;
    gif->repeat = repeat;
    gif->numColors = numColors;
    gif->palSize = palSize;

    return gif;
}

void jo_gifx_quantize_colors(jo_gifx_t *gif, uint32_t *rgba)
{
    size_t size = (size_t)gif->width * (size_t)gif->height;
    jo_gifx_quantize(rgba, size * 4, 1, gif->palette, gif->numColors);
}

bool jo_gifx_frame(jo_gifx_write_fn write_fn, void *user, jo_gifx_t *gif,
                   uint32_t *rgba, uint16_t delayCsec)
{
    uint16_t width = gif->width;
    uint16_t height = gif->height;
    size_t size = (size_t)width * (size_t)height;

    const unsigned char *palette = gif->palette;
    unsigned char *indexedPixels = gif->pixels + size * (gif->frame % 2);
    {
        unsigned char *ditheredPixels = gif->pixels + size * 2;
        for (size_t i = 0; i < size; ++i) {
            uint32_t color = rgba[i];
            ditheredPixels[i * 4 + 0] = (color >> 16) & 0xff;
            ditheredPixels[i * 4 + 1] = (color >> 8) & 0xff;
            ditheredPixels[i * 4 + 2] = color & 0xff;
            ditheredPixels[i * 4 + 3] = (color >> 24) & 0xff;
        }

        for (int k = 0; k < size * 4; k += 4) {
            int rgb[3] = {ditheredPixels[k + 0], ditheredPixels[k + 1],
                          ditheredPixels[k + 2]};
            int bestd = 0x7FFFFFFF, best = -1;
            // TODO: exhaustive search. do something better.
            for (int i = 0; i < gif->numColors; ++i) {
                int bb = palette[i * 3 + 0] - rgb[0];
                int gg = palette[i * 3 + 1] - rgb[1];
                int rr = palette[i * 3 + 2] - rgb[2];
                int d = bb * bb + gg * gg + rr * rr;
                if (d < bestd) {
                    bestd = d;
                    best = i;
                }
            }
            indexedPixels[k / 4] = best;
            int diff[3] = {
                ditheredPixels[k + 0] - palette[indexedPixels[k / 4] * 3 + 0],
                ditheredPixels[k + 1] - palette[indexedPixels[k / 4] * 3 + 1],
                ditheredPixels[k + 2] - palette[indexedPixels[k / 4] * 3 + 2]};
            // Floyd-Steinberg Error Diffusion
            // TODO: Use something better --
            // http://caca.zoy.org/study/part3.html
            if (k + 4 < size * 4) {
                ditheredPixels[k + 4 + 0] = (unsigned char)jo_gifx_clamp(
                    ditheredPixels[k + 4 + 0] + (diff[0] * 7 / 16), 0, 255);
                ditheredPixels[k + 4 + 1] = (unsigned char)jo_gifx_clamp(
                    ditheredPixels[k + 4 + 1] + (diff[1] * 7 / 16), 0, 255);
                ditheredPixels[k + 4 + 2] = (unsigned char)jo_gifx_clamp(
                    ditheredPixels[k + 4 + 2] + (diff[2] * 7 / 16), 0, 255);
            }
            if (k + width * 4 + 4 < size * 4) {
                for (int i = 0; i < 3; ++i) {
                    ditheredPixels[k - 4 + width * 4 + i] =
                        (unsigned char)jo_gifx_clamp(
                            ditheredPixels[k - 4 + width * 4 + i]
                                + (diff[i] * 3 / 16),
                            0, 255);
                    ditheredPixels[k + width * 4 + i] =
                        (unsigned char)jo_gifx_clamp(
                            ditheredPixels[k + width * 4 + i]
                                + (diff[i] * 5 / 16),
                            0, 255);
                    ditheredPixels[k + width * 4 + 4 + i] =
                        (unsigned char)jo_gifx_clamp(
                            ditheredPixels[k + width * 4 + 4 + i]
                                + (diff[i] * 1 / 16),
                            0, 255);
                }
            }
        }
    }
    unsigned char *outPixels;
    if (gif->frame == 0) {
        outPixels = indexedPixels;
        // Global Color Table
        write_fn(user, palette, 3 * (1 << (gif->palSize + 1)));
        if (gif->repeat >= 0 && gif->repeat <= UINT16_MAX) {
            unsigned char extension_block[] = {
                // Application extension header, block size
                0x21, 0xff, 0x0b,
                // "NETSCAPE2.0" in ASCII, byte count (3), block index (1)
                0x4e, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e,
                0x30, 0x03, 0x01,
                // Loop count in 16 bit little-endian, block terminator
                gif->repeat & 0xff, (gif->repeat >> 8) & 0xff, 0x00};
            write_fn(user, extension_block, sizeof(extension_block));
        }
    }
    else {
        // Pixels that are the same as the previous frame are set to transparent
        unsigned char *prevPixels = gif->pixels + size * ((gif->frame + 1) % 2);
        outPixels = gif->pixels + size * 2;
        for (size_t i = 0; i < size; ++i) {
            unsigned char p = indexedPixels[i];
            outPixels[i] = p == prevPixels[i] ? 0xff : p;
        }
    }
    unsigned char image_header[] = {
        // Graphic Control Extension
        0x21, 0xf9, 0x04, 0x01,
        // Delay in centiseconds, 16 bit little-endian
        delayCsec & 0xff, (delayCsec >> 8) & 0xff,
        // Transparent pixel index (255), end of block
        0xff, 0x00,
        // Image Descriptor header, x, y (always top-left, so zero)
        0x2c, 0x00, 0x00, 0x00, 0x00,
        // Width in 16 bit little-endian
        width & 0xff, (width >> 8) & 0xff,
        // Height in 16 bit little-endian
        height & 0xff, (height >> 8) & 0xff,
        // Local palette size (zero, because we don't use local palettes)
        0x00};
    if (!write_fn(user, image_header, sizeof(image_header))) {
        return false;
    }
    // Start of image, compressed pixels, block terminator.
    bool ok = write_fn(user, (unsigned char[]){0x08}, 1)
           && jo_gifx_lzw_encode(write_fn, user, gif, outPixels, size)
           && write_fn(user, (unsigned char[]){0x00}, 1);
    ++gif->frame;
    return ok;
}

bool jo_gifx_end(jo_gifx_write_fn write_fn, void *user, jo_gifx_t *gif)
{
    free(gif);
    return write_fn(user, (unsigned char[]){0x3b}, 1); // gif trailer
}

void jo_gifx_abort(jo_gifx_t *gif)
{
    free(gif);
}
