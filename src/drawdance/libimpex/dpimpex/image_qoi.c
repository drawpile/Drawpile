// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_qoi.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_state.h>
#include <dpengine/image.h>
#include <dpengine/pixels.h>

// SPDX-SnippetBegin
// SPDX-License-Identifier: MIT
// SDPX—SnippetName: based on the QOI reference implementation

#define DP_QOI_OP_INDEX     0x00
#define DP_QOI_OP_DIFF      0x40
#define DP_QOI_OP_LUMA      0x80
#define DP_QOI_OP_RUN       0xc0
#define DP_QOI_OP_RGB       0xfe
#define DP_QOI_OP_RGBA      0xff
#define DP_QOI_MASK_2       0xc0
#define DP_QOI_HEADER_SIZE  14
#define DP_QOI_PADDING_INIT {0, 0, 0, 0, 0, 0, 0, 1}
#define DP_QOI_MAGIC                                       \
    (((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 \
     | ((unsigned int)'i') << 8 | ((unsigned int)'f'))


static int hash_upixel8(DP_UPixel8 px)
{
    return px.r * 3 + px.g * 5 + px.b * 7 + px.a * 11;
}

DP_Image *DP_image_qoi_read(DP_Input *input)
{
    DP_ASSERT(input);

    size_t fill = 0;
    unsigned char buf[BUFSIZ];

    bool error;
    fill = DP_input_read(input, buf, DP_QOI_HEADER_SIZE, &error);
    if (error) {
        return NULL;
    }
    else if (fill < DP_QOI_HEADER_SIZE) {
        DP_error_set("Failed to read QOI header");
        return NULL;
    }

    if (DP_read_bigendian_uint32(buf) != DP_QOI_MAGIC) {
        DP_error_set("Bad QOI magic");
        return NULL;
    }

    long long width = DP_read_bigendian_uint32(buf + 4);
    long long height = DP_read_bigendian_uint32(buf + 8);
    long long pixel_count_ll;
    if (!DP_canvas_state_dimensions_in_bounds(width, height)
        || (pixel_count_ll = width * height) < 0LL
        || pixel_count_ll > (long long)INT_MAX) {
        DP_error_set("QOI dimensions %lldx%lld out of bounds", width, height);
        return NULL;
    }

    DP_Image *img =
        DP_image_new(DP_llong_to_int(width), DP_llong_to_int(height));
    DP_Pixel8 *dst = DP_image_pixels(img);
    int pixel_count = DP_llong_to_int(pixel_count_ll);

    int run = 0;
    DP_UPixel8 px = {.b = 0, .g = 0, .r = 0, .a = 255};
    DP_UPixel8 index[64];
    memset(index, 0, sizeof(index));

    fill = 0;
    size_t pos = 0;
#define DP_QOI_READ_NEXT_BYTE(OUT)                                 \
    do {                                                           \
        if (pos < fill) {                                          \
            OUT = buf[pos++];                                      \
        }                                                          \
        else {                                                     \
            fill = DP_input_read(input, buf, sizeof(buf), &error); \
            if (error) {                                           \
                DP_image_free(img);                                \
                return NULL;                                       \
            }                                                      \
            else if (fill == 0) {                                  \
                DP_image_free(img);                                \
                DP_error_set("QOI: premature end of file");        \
                return NULL;                                       \
            }                                                      \
            else {                                                 \
                pos = 0;                                           \
                OUT = buf[pos++];                                  \
            }                                                      \
        }                                                          \
    } while (0)

    for (int i = 0; i < pixel_count; ++i) {
        if (run > 0) {
            --run;
            dst[i] = dst[i - 1];
        }
        else {
            int b1;
            DP_QOI_READ_NEXT_BYTE(b1);

            if (b1 == DP_QOI_OP_RGB) {
                DP_QOI_READ_NEXT_BYTE(px.r);
                DP_QOI_READ_NEXT_BYTE(px.g);
                DP_QOI_READ_NEXT_BYTE(px.b);
            }
            else if (b1 == DP_QOI_OP_RGBA) {
                DP_QOI_READ_NEXT_BYTE(px.r);
                DP_QOI_READ_NEXT_BYTE(px.g);
                DP_QOI_READ_NEXT_BYTE(px.b);
                DP_QOI_READ_NEXT_BYTE(px.a);
            }
            else if ((b1 & DP_QOI_MASK_2) == DP_QOI_OP_INDEX) {
                px = index[b1];
            }
            else if ((b1 & DP_QOI_MASK_2) == DP_QOI_OP_DIFF) {
                px.r = (uint8_t)(px.r + (((b1 >> 4) & 0x03) - 2));
                px.g = (uint8_t)(px.g + (((b1 >> 2) & 0x03) - 2));
                px.b = (uint8_t)(px.b + ((b1 & 0x03) - 2));
            }
            else if ((b1 & DP_QOI_MASK_2) == DP_QOI_OP_LUMA) {
                int b2;
                DP_QOI_READ_NEXT_BYTE(b2);
                int vg = (b1 & 0x3f) - 32;
                px.r = (uint8_t)(px.r + (vg - 8 + ((b2 >> 4) & 0x0f)));
                px.g = (uint8_t)(px.g + vg);
                px.b = (uint8_t)(px.b + (vg - 8 + (b2 & 0x0f)));
            }
            else if ((b1 & DP_QOI_MASK_2) == DP_QOI_OP_RUN) {
                run = (b1 & 0x3f);
            }

            index[hash_upixel8(px) & (64 - 1)] = px;
            dst[i] = DP_pixel8_premultiply(px);
        }
    }

    unsigned char padding[] = DP_QOI_PADDING_INIT;
    for (int i = 0; i < (int)sizeof(padding); ++i) {
        unsigned char b;
        DP_QOI_READ_NEXT_BYTE(b);
        if (b != padding[i]) {
            DP_image_free(img);
            DP_error_set("Bad QOI end marker byte %d, want %d, got %d", i,
                         (int)padding[i], (int)b);
            return NULL;
        }
    }

    return img;
}

bool DP_image_qoi_write(DP_Output *output, int width, int height,
                        DP_Pixel8 *pixels)
{
    DP_ASSERT(output);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);
    DP_ASSERT(pixels);

    if (!DP_OUTPUT_WRITE_BIGENDIAN(output, DP_OUTPUT_UINT32(DP_QOI_MAGIC),
                                   DP_OUTPUT_UINT32(width),
                                   DP_OUTPUT_UINT32(height),
                                   DP_OUTPUT_UINT8(4), // channels
                                   DP_OUTPUT_UINT8(0), // color space (SRGB)
                                   DP_OUTPUT_END)) {
        return false;
    }

    int run = 0;
    DP_UPixel8 px_prev = {.b = 0, .g = 0, .r = 0, .a = 255};
    DP_UPixel8 index[64];
    memset(index, 0, sizeof(index));

    int pixel_count = width * height;
    for (int i = 0; i < pixel_count; ++i) {
        DP_UPixel8 px = DP_pixel8_unpremultiply(pixels[i]);
        size_t len = 0;
        unsigned char buf[6];

        if (px.color == px_prev.color) {
            ++run;
            if (run == 62 || i == pixel_count - 1) {
                buf[len++] = DP_int_to_uchar(DP_QOI_OP_RUN | (run - 1));
                run = 0;
            }
        }
        else {
            if (run > 0) {
                buf[len++] = DP_int_to_uchar(DP_QOI_OP_RUN | (run - 1));
                run = 0;
            }

            int index_pos = hash_upixel8(px) & (64 - 1);
            if (index[index_pos].color == px.color) {
                buf[len++] = DP_int_to_uchar(DP_QOI_OP_INDEX | index_pos);
            }
            else {
                index[index_pos] = px;

                if (px.a == px_prev.a) {
                    signed char vr = (signed char)(px.r - px_prev.r);
                    signed char vg = (signed char)(px.g - px_prev.g);
                    signed char vb = (signed char)(px.b - px_prev.b);
                    signed char vg_r = vr - vg;
                    signed char vg_b = vb - vg;

                    if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3
                        && vb < 2) {
                        buf[len++] =
                            DP_int_to_uchar(DP_QOI_OP_DIFF | (vr + 2) << 4
                                            | (vg + 2) << 2 | (vb + 2));
                    }
                    else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32
                             && vg_b > -9 && vg_b < 8) {
                        buf[len++] =
                            DP_int_to_uchar(DP_QOI_OP_LUMA | (vg + 32));
                        buf[len++] =
                            DP_int_to_uchar((vg_r + 8) << 4 | (vg_b + 8));
                    }
                    else {
                        buf[len++] = DP_QOI_OP_RGB;
                        buf[len++] = px.r;
                        buf[len++] = px.g;
                        buf[len++] = px.b;
                    }
                }
                else {
                    buf[len++] = DP_QOI_OP_RGBA;
                    buf[len++] = px.r;
                    buf[len++] = px.g;
                    buf[len++] = px.b;
                    buf[len++] = px.a;
                }
            }
        }

        if (!DP_output_write(output, buf, len)) {
            return false;
        }

        px_prev = px;
    }

    unsigned char padding[] = DP_QOI_PADDING_INIT;
    return DP_output_write(output, padding, sizeof(padding))
        && DP_output_flush(output);
}

// SPDX-SnippetEnd
