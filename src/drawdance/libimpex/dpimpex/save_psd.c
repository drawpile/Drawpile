// SPDX-License-Identifier: GPL-3.0-or-later
#include "save_psd.h"
#include "utf16be.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/layer_content.h>
#include <dpengine/layer_group.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpengine/pixels.h>
#include <dpmsg/blend_mode.h>
#include <ctype.h>
#include <uthash_inc.h>


typedef enum DP_SavePsdSection {
    DP_SAVE_PSD_SECTION_OTHER = 0,
    DP_SAVE_PSD_SECTION_OPEN_FOLDER = 1,
    DP_SAVE_PSD_SECTION_DIVIDER = 3,
} DP_SavePsdSection;

typedef struct DP_SavePsdLayerOffset {
    DP_LayerProps *lp;
    size_t pos;
    UT_hash_handle hh;
} DP_SavePsdLayerOffset;

typedef struct DP_SavePsdLayerOffsets {
    size_t background_pos;
    DP_SavePsdLayerOffset *map;
} DP_SavePsdLayerOffsets;

static bool set_background_layer_offset(DP_SavePsdLayerOffsets *layer_offsets,
                                        DP_Output *out)
{
    bool error;
    size_t pos = DP_output_tell(out, &error);
    if (error) {
        return false;
    }
    else {
        layer_offsets->background_pos = pos;
        return true;
    }
}

static bool insert_layer_offset(DP_SavePsdLayerOffsets *layer_offsets,
                                DP_LayerProps *lp, DP_Output *out)
{
    bool error;
    size_t pos = DP_output_tell(out, &error);
    if (error) {
        return false;
    }
    else {
        DP_SavePsdLayerOffset *offset = DP_malloc(sizeof(*offset));
        offset->lp = lp;
        offset->pos = pos;
        HASH_ADD_PTR(layer_offsets->map, lp, offset);
        return true;
    }
}

static size_t get_layer_offset(DP_SavePsdLayerOffsets *layer_offsets,
                               DP_LayerProps *lp)
{
    DP_SavePsdLayerOffset *offset;
    HASH_FIND_PTR(layer_offsets->map, &lp, offset);
    return offset->pos;
}

static void free_layer_offsets(DP_SavePsdLayerOffsets *layer_offsets)
{
    DP_SavePsdLayerOffset *map = layer_offsets->map;
    DP_SavePsdLayerOffset *current, *tmp;
    HASH_ITER(hh, map, current, tmp) {
        HASH_DEL(map, current);
        DP_free(current);
    }
}


static bool write_preliminary_size_prefix(DP_Output *out, size_t *out_start)
{
    bool error;
    *out_start = DP_output_tell(out, &error);
    return !error && DP_OUTPUT_WRITE_BYTES_LITERAL(out, 0, 0, 0, 0);
}

static bool write_layer_title(DP_DrawContext *dc, DP_Output *out,
                              const char *title)
{
    // Should only really need 256 bytes, but the pool is 8K at minimum anyway.
    unsigned char *buffer = DP_draw_context_pool_require(dc, 280);
    size_t len = DP_min_size(255, strlen(title));
    buffer[0] = DP_size_to_uint8(len);
    for (size_t i = 0; i < len; ++i) {
        char c = title[i];
        buffer[i + 1] = DP_char_to_uchar(isascii(c) ? c : '_');
    }

    size_t size = len + 1;
    switch (size % 4) {
    case 1:
        buffer[size++] = 0;
        DP_FALLTHROUGH();
    case 2:
        buffer[size++] = 0;
        DP_FALLTHROUGH();
    case 3:
        buffer[size++] = 0;
        DP_FALLTHROUGH();
    default:
        break;
    }

    return DP_output_write(out, buffer, size);
}

static bool set_utf16be_layer_title(void *user, const char *bytes,
                                    size_t bytes_length)
{
    DP_DrawContext *dc = ((void **)user)[0];
    char *buffer = DP_draw_context_pool_require(dc, bytes_length + 4);
    memcpy(buffer, bytes, bytes_length);
    memset(buffer + bytes_length, 0, 3); // 16 bit NUL and padding byte.
    size_t *out_bytes_length = ((void **)user)[1];
    *out_bytes_length = bytes_length;
    return true;
}

static bool write_layer_utf16be_title(DP_DrawContext *dc, DP_Output *out,
                                      const char *title)
{
    size_t bytes_length;
    char *bytes;
    bool conversion_ok = DP_utf8_to_utf16be(title, set_utf16be_layer_title,
                                            (void *[]){dc, &bytes_length});
    if (conversion_ok) {
        bytes = DP_draw_context_pool(dc);
    }
    else {
        bytes_length = 0;
        bytes = DP_draw_context_pool_require(dc, 3);
        memset(bytes, 0, 3); // 16 bit NUL and padding byte.
    }

    size_t block_len = 4 + bytes_length + 2;
    bool needs_pad = block_len % 2 != 0;
    return DP_OUTPUT_WRITE_BIGENDIAN(
               out,
               // "8BIM" magic number.
               DP_OUTPUT_BYTES_LITERAL(56, 66, 73, 77),
               // "luni" block key.
               DP_OUTPUT_BYTES_LITERAL(108, 117, 110, 105),
               // Block length.
               DP_OUTPUT_UINT32(
                   DP_size_to_uint32(block_len + (needs_pad ? 1 : 0))),
               // Length in 16 bit units.
               DP_OUTPUT_UINT32(DP_size_to_uint32(bytes_length / 2)),
               DP_OUTPUT_END)
        // The title itself in UTF16-BE, NUL and potentially a pad byte.
        && DP_output_write(out, bytes, bytes_length + (needs_pad ? 3 : 2));
}

static bool write_size_prefix(DP_Output *out, size_t start, size_t alignment)
{
    DP_ASSERT(alignment <= 2);
    bool error;
    size_t end = DP_output_tell(out, &error);
    size_t actual_end;
    if (alignment == 0) {
        actual_end = end;
    }
    else {
        size_t mask = alignment - 1;
        actual_end = (end + mask) & ~mask;
        if (!DP_output_write(out, (unsigned char[]){0, 0, 0, 0, 0, 0, 0, 0, 0},
                             actual_end - end)) {
            return false;
        }
    }
    return DP_output_seek(out, start)
        && DP_OUTPUT_WRITE_BIGENDIAN(
               out, DP_OUTPUT_UINT32(DP_size_to_uint32(actual_end - start - 4)),
               DP_OUTPUT_END)
        && DP_output_seek(out, actual_end);
}

static int count_layers_recursive(DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    int total = count;
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            // PSD needs two layers for every group.
            total += 1 + count_layers_recursive(child_lpl);
        }
    }
    return total;
}

static const char *blend_mode_to_psd(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_DARKEN:
    case DP_BLEND_MODE_DARKEN_ALPHA:
        return "dark";
    case DP_BLEND_MODE_MULTIPLY:
    case DP_BLEND_MODE_MULTIPLY_ALPHA:
        return "mul ";
    case DP_BLEND_MODE_BURN:
    case DP_BLEND_MODE_BURN_ALPHA:
        return "idiv";
    case DP_BLEND_MODE_LINEAR_BURN:
    case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
        return "lbrn";
    case DP_BLEND_MODE_LIGHTEN:
    case DP_BLEND_MODE_LIGHTEN_ALPHA:
        return "lite";
    case DP_BLEND_MODE_SCREEN:
    case DP_BLEND_MODE_SCREEN_ALPHA:
        return "scrn";
    case DP_BLEND_MODE_DODGE:
    case DP_BLEND_MODE_DODGE_ALPHA:
        return "div ";
    case DP_BLEND_MODE_ADD:
    case DP_BLEND_MODE_ADD_ALPHA:
        return "lddg";
    case DP_BLEND_MODE_OVERLAY:
    case DP_BLEND_MODE_OVERLAY_ALPHA:
        return "over";
    case DP_BLEND_MODE_SOFT_LIGHT:
    case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
        return "sLit";
    case DP_BLEND_MODE_HARD_LIGHT:
    case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
        return "hLit";
    case DP_BLEND_MODE_LINEAR_LIGHT:
    case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
        return "lLit";
    case DP_BLEND_MODE_SUBTRACT:
    case DP_BLEND_MODE_SUBTRACT_ALPHA:
        return "fsub";
    case DP_BLEND_MODE_DIVIDE:
    case DP_BLEND_MODE_DIVIDE_ALPHA:
        return "fdiv";
    case DP_BLEND_MODE_HUE:
    case DP_BLEND_MODE_HUE_ALPHA:
        return "hue ";
    case DP_BLEND_MODE_SATURATION:
    case DP_BLEND_MODE_SATURATION_ALPHA:
        return "sat ";
    case DP_BLEND_MODE_COLOR:
    case DP_BLEND_MODE_COLOR_ALPHA:
        return "colr";
    case DP_BLEND_MODE_LUMINOSITY:
    case DP_BLEND_MODE_LUMINOSITY_ALPHA:
        return "lum ";
    default:
        return "norm";
    }
}

static uint8_t layer_section_flags(bool hidden, DP_SavePsdSection section)
{
    uint8_t hidden_flag = hidden ? 0x2 : 0x0;
    switch (section) {
    case DP_SAVE_PSD_SECTION_DIVIDER:
    case DP_SAVE_PSD_SECTION_OPEN_FOLDER:
        return (uint8_t)0x10 | hidden_flag;
    case DP_SAVE_PSD_SECTION_OTHER:
        return hidden_flag;
    }
    DP_UNREACHABLE();
}

static bool needs_lsct_block(DP_SavePsdSection section)
{
    switch (section) {
    case DP_SAVE_PSD_SECTION_DIVIDER:
    case DP_SAVE_PSD_SECTION_OPEN_FOLDER:
        return true;
    case DP_SAVE_PSD_SECTION_OTHER:
        return false;
    }
    DP_UNREACHABLE();
}

static bool write_lsct_block(DP_Output *out, DP_SavePsdSection section,
                             bool isolated, const char *psd_blend_mode)
{
    unsigned char value = (unsigned char)section;
    return DP_OUTPUT_WRITE_BYTES_LITERAL(
               out, 56, 66, 73, 77, // "8BIM" magic number.
               108, 115, 99, 116,   // "lsct" block key.
               0, 0, 0, 12,         // Block content size.
               0, 0, 0, value,      // Section type.
               56, 66, 73, 77)      // "8BIM" magic number again.
        && DP_output_print(out, isolated ? psd_blend_mode : "pass");
}

static bool write_layer_info(DP_DrawContext *dc, DP_Output *out, int blend_mode,
                             uint8_t opacity, bool hidden, const char *title,
                             DP_SavePsdSection section, bool isolated,
                             bool clip)
{
    size_t section_start;
    const char *psd_blend_mode;
    // We write placeholders for an empty layer. If the layer has content, it
    // will be overwritten later. Channel size is 2, since they have a header.
    return DP_OUTPUT_WRITE_BYTES_LITERAL(
               out, 0, 0, 0, 0, 0, 0, 0, 0, // Top, left.
               0, 0, 0, 0, 0, 0, 0, 0,      // Bottom, right.
               0, 4,                        // Channel count, 4 for ARGB.
               255, 255, 0, 0, 0, 2,        // Alpha channel, id -1, size 2.
               0, 0, 0, 0, 0, 2,            // Red channel, id 0, size 2.
               0, 1, 0, 0, 0, 2,            // Blue channel, id 1, size 2.
               0, 2, 0, 0, 0, 2,            // Green channel id 2, size 2.
               56, 66, 73, 77)              // "8BIM" magic number.
        // Blend mode.
        && DP_output_print(out,
                           (psd_blend_mode = blend_mode_to_psd(blend_mode)))
        // Opacity, clipping, flags, reserved byte.
        && DP_OUTPUT_WRITE_BIGENDIAN(
               out, DP_OUTPUT_UINT8(opacity), DP_OUTPUT_UINT8(clip ? 1 : 0),
               DP_OUTPUT_UINT8(layer_section_flags(hidden, section)),
               DP_OUTPUT_UINT8(0), DP_OUTPUT_END)
        // Extra layer data section.
        && write_preliminary_size_prefix(out, &section_start)
        // Layer mask (none) and blending ranges (also none)
        && DP_OUTPUT_WRITE_BYTES_LITERAL(out, 0, 0, 0, 0, 0, 0, 0, 0)
        // Title, in ASCII and UTF16-BE.
        && write_layer_title(dc, out, title)
        && write_layer_utf16be_title(dc, out, title)
        // Section block, if the section type requires one.
        && (!needs_lsct_block(section)
            || write_lsct_block(out, section, isolated, psd_blend_mode))
        // Fill in extra layer data section size.
        && write_size_prefix(out, section_start, 0);
}

static bool write_layer_props_info(DP_LayerProps *lp, DP_DrawContext *dc,
                                   DP_Output *out, DP_SavePsdSection section,
                                   bool isolated)
{
    int blend_mode = DP_layer_props_blend_mode(lp);
    uint8_t opacity = DP_channel15_to_8(DP_layer_props_opacity(lp));
    bool hidden = DP_layer_props_hidden(lp);
    const char *title = section == DP_SAVE_PSD_SECTION_DIVIDER
                          ? "</Layer group>"
                          : DP_layer_props_title(lp, NULL);
    bool clip = DP_layer_props_clip(lp);
    return write_layer_info(dc, out, blend_mode, opacity, hidden, title,
                            section, isolated, clip);
}

static bool write_layer_infos_recursive(DP_LayerPropsList *lpl,
                                        DP_DrawContext *dc, DP_Output *out,
                                        DP_SavePsdLayerOffsets *layer_offsets)
{
    int count = DP_layer_props_list_count(lpl);
    bool ok = true;
    for (int i = 0; ok && i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            bool isolated = DP_layer_props_isolated(lp);
            ok = write_layer_props_info(lp, dc, out,
                                        DP_SAVE_PSD_SECTION_DIVIDER, isolated)
              && write_layer_infos_recursive(child_lpl, dc, out, layer_offsets)
              && write_layer_props_info(
                     lp, dc, out, DP_SAVE_PSD_SECTION_OPEN_FOLDER, isolated);
        }
        else {
            ok = insert_layer_offset(layer_offsets, lp, out)
              && write_layer_props_info(lp, dc, out, DP_SAVE_PSD_SECTION_OTHER,
                                        true);
        }
    }
    return ok;
}

static void extract_channel(size_t stride, const DP_UPixel8 *pixels,
                            uint8_t *src, uint8_t (*extract)(DP_UPixel8))
{
    for (size_t i = 0; i < stride; ++i) {
        src[i] = extract(pixels[i]);
    }
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-2.0-or-later
// SDPX—SnippetName: PSD RLE compression from Krita, originally from GIMP
static size_t rle_compress(size_t stride, const uint8_t *src,
                           unsigned char *dst)
{
    int remaining = DP_size_to_int(stride);
    int length = 0;
    int start = 0;
    int dst_ptr = 0;
    memset(dst, 0, stride * 2);

    while (remaining > 0) {
        // Look for characters matching the first.
        int i = 0;
        while (i < 128 && remaining - i > 0 && src[start] == src[start + i]) {
            ++i;
        }

        if (i > 1) {
            // Match found.
            dst[dst_ptr] = (unsigned char)(-(i - 1));
            dst[dst_ptr + 1] = src[start];
            dst_ptr += 2;
            start += i;
            remaining -= i;
            length += 2;
        }
        else {
            // Look for characters different from the previous.
            i = 0;
            while ((i < 128) && (remaining - (i + 1) > 0)
                   && (src[start + i] != src[start + i + 1]
                       || remaining - (i + 2) <= 0
                       || src[start + i] != src[start + i + 2])) {
                ++i;
            }

            // If there's only 1 remaining, the previous WHILE stmt doesn't
            // catch it
            if (remaining == 1) {
                i = 1;
            }

            if (i > 0) {
                dst[dst_ptr] = (unsigned char)(i - 1);
                int si = i;
                for (int j = 0; j < si; ++j) {
                    dst[dst_ptr + 1 + j] = src[start + j];
                }
                dst_ptr += 1 + si;
                start += si;
                remaining -= i;
                length += i + 1;
            }
        }
    }

    return DP_int_to_size(length);
}
// SPDX-SnippetEnd

static bool write_channel(DP_Output *out, size_t rows, size_t stride,
                          const DP_UPixel8 *pixels, size_t counts_length,
                          unsigned char *counts, uint8_t *src,
                          unsigned char *dst, uint8_t (*extract)(DP_UPixel8),
                          size_t *out_pos)
{
    bool error;
    size_t start_pos = DP_output_tell(out, &error);
    if (error || !DP_OUTPUT_WRITE_BYTES_LITERAL(out, 0, 1)) {
        return false;
    }

    memset(counts, 0, counts_length);
    if (!DP_output_write(out, counts, counts_length)) {
        return false;
    }

    for (size_t i = 0; i < rows; ++i) {
        size_t offset = i * stride;
        extract_channel(stride, pixels + offset, src, extract);

        size_t length = rle_compress(stride, src, dst);
        if (!DP_output_write(out, dst, length)) {
            return false;
        }

        DP_write_bigendian_uint16(DP_size_to_uint16(length), counts + i * 2);
    }

    size_t end_pos = DP_output_tell(out, &error);
    if (error) {
        return false;
    }

    *out_pos = end_pos - start_pos;
    return DP_output_seek(out, start_pos + 2)
        && DP_output_write(out, counts, counts_length)
        && DP_output_seek(out, end_pos);
}

static uint8_t get_upixel8_a(DP_UPixel8 pixel)
{
    return pixel.a;
}

static uint8_t get_upixel8_r(DP_UPixel8 pixel)
{
    return pixel.r;
}

static uint8_t get_upixel8_g(DP_UPixel8 pixel)
{
    return pixel.g;
}

static uint8_t get_upixel8_b(DP_UPixel8 pixel)
{
    return pixel.b;
}

static bool write_pixel_channels(DP_DrawContext *dc, DP_Output *out,
                                 size_t rows, size_t stride,
                                 const DP_UPixel8 *pixels, size_t *out_a,
                                 size_t *out_r, size_t *out_g, size_t *out_b)
{
    size_t counts_length = rows * 2;
    size_t pool_size = counts_length + stride * 3;
    unsigned char *counts = DP_draw_context_pool_require(dc, pool_size);
    uint8_t *src = counts + counts_length;
    unsigned char *dst = src + stride;
    return write_channel(out, rows, stride, pixels, counts_length, counts, src,
                         dst, get_upixel8_a, out_a)
        && write_channel(out, rows, stride, pixels, counts_length, counts, src,
                         dst, get_upixel8_r, out_r)
        && write_channel(out, rows, stride, pixels, counts_length, counts, src,
                         dst, get_upixel8_g, out_g)
        && write_channel(out, rows, stride, pixels, counts_length, counts, src,
                         dst, get_upixel8_b, out_b);
}

static bool write_pixel_data(DP_DrawContext *dc, DP_Output *out, size_t pos,
                             const DP_UPixel8 *pixels, int offset_x,
                             int offset_y, int width, int height)
{
    if (pixels && width > 0 && height > 0) {
        // We got some pixel data, run-length encode it. That's the default in
        // Photoshop apparently and Krita also always uses this option.
        size_t rows = DP_int_to_size(height);
        size_t stride = DP_int_to_size(width);
        size_t a, r, g, b;
        if (!write_pixel_channels(dc, out, rows, stride, pixels, &a, &r, &g,
                                  &b)) {
            return false;
        }

        // Go back and fill in the channel size information.
        bool error;
        size_t end_pos = DP_output_tell(out, &error);
        return !error && DP_output_seek(out, pos)
            && DP_OUTPUT_WRITE_BIGENDIAN(
                out,
                // Bounding rectangle.
                DP_OUTPUT_UINT32(DP_int_to_uint32(offset_y)),
                DP_OUTPUT_UINT32(DP_int_to_uint32(offset_x)),
                DP_OUTPUT_UINT32(DP_int_to_uint32(offset_y + height)),
                DP_OUTPUT_UINT32(DP_int_to_uint32(offset_x + width)),
                // Number of channels, always 4 for ARGB.
                DP_OUTPUT_UINT16(4),
                // Channel ids and the sizes of their pixel data.
                DP_OUTPUT_INT16(-1), DP_OUTPUT_UINT32(a), DP_OUTPUT_INT16(0),
                DP_OUTPUT_UINT32(r), DP_OUTPUT_INT16(1), DP_OUTPUT_UINT32(g),
                DP_OUTPUT_INT16(2), DP_OUTPUT_UINT32(b), DP_OUTPUT_END)
            && DP_output_seek(out, end_pos);
    }
    else {
        // Group or empty layer. Just write blank pixel data.
        return DP_OUTPUT_WRITE_BYTES_LITERAL(out, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

static bool write_background_pixel_data(DP_CanvasState *cs, DP_DrawContext *dc,
                                        DP_Output *out,
                                        DP_SavePsdLayerOffsets *layer_offsets)
{
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
        width, height, DP_canvas_state_background_tile_noinc(cs));
    DP_UPixel8 *pixels = DP_layer_content_to_upixels8((DP_LayerContent *)tlc, 0,
                                                      0, width, height);
    DP_transient_layer_content_decref(tlc);
    return write_pixel_data(dc, out, layer_offsets->background_pos, pixels, 0,
                            0, width, height);
}

static bool write_layer_content_pixel_data(DP_LayerContent *lc, bool censored,
                                           DP_DrawContext *dc, DP_Output *out,
                                           size_t pos)
{
    int offset_x, offset_y, width, height;
    DP_UPixel8 *pixels = DP_layer_content_to_upixels8_cropped(
        lc, censored, &offset_x, &offset_y, &width, &height);
    bool ok = write_pixel_data(dc, out, pos, pixels, offset_x, offset_y, width,
                               height);
    DP_free(pixels);
    return ok;
}

static bool write_layer_pixel_data_recursive(
    DP_LayerList *ll, DP_LayerPropsList *lpl, DP_DrawContext *dc,
    DP_Output *out, DP_SavePsdLayerOffsets *layer_offsets, bool parent_censored)
{
    int count = DP_layer_props_list_count(lpl);
    bool ok = true;
    for (int i = 0; ok && i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            ok = write_pixel_data(dc, out, 0, NULL, 0, 0, 0, 0)
              && write_layer_pixel_data_recursive(
                     DP_layer_group_children_noinc(
                         DP_layer_list_group_at_noinc(ll, i)),
                     child_lpl, dc, out, layer_offsets,
                     DP_layer_props_censored(lp))
              && write_pixel_data(dc, out, 0, NULL, 0, 0, 0, 0);
        }
        else {
            ok = write_layer_content_pixel_data(
                DP_layer_list_content_at_noinc(ll, i),
                parent_censored || DP_layer_props_censored(lp), dc, out,
                get_layer_offset(layer_offsets, lp));
        }
    }
    return ok;
}

static bool
write_layer_pixel_data_section(DP_CanvasState *cs, DP_DrawContext *dc,
                               DP_Output *out,
                               DP_SavePsdLayerOffsets *layer_offsets)
{
    return write_background_pixel_data(cs, dc, out, layer_offsets)
        && write_layer_pixel_data_recursive(
               DP_canvas_state_layers_noinc(cs),
               DP_canvas_state_layer_props_noinc(cs), dc, out, layer_offsets,
               false);
}

static bool write_layer_info_section(DP_CanvasState *cs, DP_DrawContext *dc,
                                     DP_Output *out,
                                     DP_SavePsdLayerOffsets *layer_offsets)
{
    size_t section_start;
    if (!write_preliminary_size_prefix(out, &section_start)) {
        return false;
    }

    // Number of layers, plus one for the background. As a negative number
    // because, uh, something about alpha channels. GIMP requires that anyway.
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int layer_count = -(count_layers_recursive(lpl) + 1);
    return layer_count >= INT16_MIN
        && DP_OUTPUT_WRITE_BIGENDIAN(
               out, DP_OUTPUT_INT16(DP_int_to_int16(layer_count)),
               DP_OUTPUT_END)
        // Background layer info.
        && set_background_layer_offset(layer_offsets, out)
        && write_layer_info(dc, out, DP_BLEND_MODE_NORMAL, 255, false,
                            "Background", DP_SAVE_PSD_SECTION_OTHER, true,
                            false)
        // Remaining layer info.
        && write_layer_infos_recursive(lpl, dc, out, layer_offsets)
        // Channel pixel data.
        && write_layer_pixel_data_section(cs, dc, out, layer_offsets)
        // Fill in section size.
        && write_size_prefix(out, section_start, 2);
}

static bool write_layer_sections(DP_CanvasState *cs, DP_DrawContext *dc,
                                 DP_Output *out)
{
    size_t section_start;
    if (!write_preliminary_size_prefix(out, &section_start)) {
        return false;
    }

    DP_SavePsdLayerOffsets layer_offsets = {0, NULL};
    bool ok = write_layer_info_section(cs, dc, out, &layer_offsets)
           // Global layer mask info (none).
           && DP_OUTPUT_WRITE_BYTES_LITERAL(out, 0, 0, 0, 0)
           && write_size_prefix(out, section_start, 0);
    free_layer_offsets(&layer_offsets);
    return ok;
}

static bool write_merged_channel(DP_Output *out, size_t rows, size_t stride,
                                 unsigned char *counts, const uint8_t *src,
                                 unsigned char *dst)
{
    for (size_t i = 0; i < rows; ++i) {
        size_t src_start = i * stride;
        size_t length = rle_compress(stride, src + src_start, dst);
        if (!DP_output_write(out, dst, length)) {
            return false;
        }
        DP_write_bigendian_uint16(DP_size_to_uint16(length), counts + i * 2);
    }
    return true;
}

static bool write_merged_image(DP_CanvasState *cs, DP_DrawContext *dc,
                               DP_Output *out)
{
    // Compression type: run-length encoding.
    if (!DP_OUTPUT_WRITE_BYTES_LITERAL(out, 0, 1)) {
        return false;
    }

    size_t rows = DP_int_to_size(DP_canvas_state_height(cs));
    size_t stride = DP_int_to_size(DP_canvas_state_width(cs));
    size_t counts_stride = rows * 2;
    size_t counts_length = counts_stride * 4;
    size_t pool_size = counts_length + stride * 2;
    unsigned char *counts = DP_draw_context_pool_require(dc, pool_size);
    unsigned char *dst = counts + counts_length;

    bool error;
    size_t counts_pos = DP_output_tell(out, &error);
    if (error) {
        return false;
    }

    memset(counts, 0, counts_length);
    if (!DP_output_write(out, counts, counts_length)) {
        return false;
    }

    size_t size = rows * stride;
    uint8_t *buffer = DP_malloc(size * 4);
    if (!DP_canvas_state_to_flat_separated_urgba8(
            cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL, buffer)) {
        DP_free(buffer);
        return false;
    }

    for (size_t i = 0; i < 4; ++i) {
        size_t counts_start = i * counts_stride;
        size_t buffer_start = i * size;
        if (!write_merged_channel(out, rows, stride, counts + counts_start,
                                  buffer + buffer_start, dst)) {
            DP_free(buffer);
            return false;
        }
    }

    DP_free(buffer);

    return DP_output_seek(out, counts_pos)
        && DP_output_write(out, counts, counts_length);
}

static bool write_psd(DP_CanvasState *cs, DP_DrawContext *dc, DP_Output *out)
{
    return DP_OUTPUT_WRITE_BIGENDIAN(
               out,
               // "8BPS" magic number.
               DP_OUTPUT_BYTES_LITERAL(56, 66, 80, 83),
               // Version, always 1.
               DP_OUTPUT_UINT16(1),
               // Reserved, always a bunch of zeroes.
               DP_OUTPUT_BYTES_LITERAL(0, 0, 0, 0, 0, 0),
               // Number of channels, we always use RGBA, so it's 4.
               DP_OUTPUT_UINT16(4),
               // Dimensions, height first for some reason. PSD supports at most
               // 30,000 pixels in either direction, more than the uint16 we do.
               DP_OUTPUT_UINT32(DP_int_to_uint32(DP_canvas_state_height(cs))),
               DP_OUTPUT_UINT32(DP_int_to_uint32(DP_canvas_state_width(cs))),
               // Channel depth, we use 8 bit channels.
               DP_OUTPUT_UINT16(8),
               // Color mode, we always use 3 for RGB.
               DP_OUTPUT_UINT16(3),
               // Color mode section length, always zero for RGB.
               DP_OUTPUT_UINT32(0),
               // Image resources section length, we don't have any.
               DP_OUTPUT_UINT32(0),
               // End of header.
               DP_OUTPUT_END)
        && write_layer_sections(cs, dc, out) && write_merged_image(cs, dc, out)
        && DP_output_flush(out);
}

DP_SaveResult DP_save_psd(DP_CanvasState *cs, const char *path,
                          DP_DrawContext *dc)
{
    DP_Output *out = DP_file_output_new_from_path(path);
    if (out) {
        bool write_ok = write_psd(cs, dc, out);
        bool free_ok = DP_output_free(out);
        return write_ok && free_ok ? DP_SAVE_RESULT_SUCCESS
                                   : DP_SAVE_RESULT_WRITE_ERROR;
    }
    else {
        return DP_SAVE_RESULT_OPEN_ERROR;
    }
}
