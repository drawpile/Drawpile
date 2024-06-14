/*
 * Copyright (C) 2022 askmeaboufoom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#include "save.h"
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_state.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "image.h"
#include "image_png.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "save_psd.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include "view_mode.h"
#include "zip_archive.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/file.h>
#include <dpcommon/geom.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpmsg/blend_mode.h>
#include <ctype.h>
#include <jo_gifx.h>
#include <limits.h>
#include <math.h>
#include <uthash_inc.h>

#define DP_PERF_CONTEXT "save"


const DP_SaveFormat *DP_save_supported_formats(void)
{
    static const char *ora_ext[] = {"ora", NULL};
    static const char *png_ext[] = {"png", NULL};
    static const char *jpeg_ext[] = {"jpg", "jpeg", NULL};
    static const char *psd_ext[] = {"psd", NULL};
    static const DP_SaveFormat formats[] = {
        {"OpenRaster", ora_ext},         {"PNG", png_ext}, {"JPEG", jpeg_ext},
        {"Photoshop Document", psd_ext}, {NULL, NULL},
    };
    return formats;
}


DP_SaveImageType DP_save_image_type_guess(const char *path)
{
    if (!path) {
        DP_error_set("Null path given");
        return DP_SAVE_IMAGE_UNKNOWN;
    }

    const char *dot = strrchr(path, '.');
    if (!dot) {
        DP_error_set("No file extension to guess image type from: %s", path);
        return DP_SAVE_IMAGE_UNKNOWN;
    }

    const char *ext = dot + 1;
    if (DP_str_equal_lowercase(ext, "ora")) {
        return DP_SAVE_IMAGE_ORA;
    }
    else if (DP_str_equal_lowercase(ext, "png")) {
        return DP_SAVE_IMAGE_PNG;
    }
    else if (DP_str_equal_lowercase(ext, "jpg")
             || DP_str_equal_lowercase(ext, "jpeg")) {
        return DP_SAVE_IMAGE_JPEG;
    }
    else if (DP_str_equal_lowercase(ext, "psd")) {
        return DP_SAVE_IMAGE_PSD;
    }
    else {
        DP_error_set("Unknown image format in '%s'", path);
        return DP_SAVE_IMAGE_UNKNOWN;
    }
}


typedef struct DP_SaveOraLayer {
    int layer_id;
    int index;
    int offset_x, offset_y;
    UT_hash_handle hh;
} DP_SaveOraLayer;

typedef struct DP_SaveOraContext {
    DP_ZipWriter *zw;
    DP_SaveOraLayer *layers;
    struct {
        size_t capacity;
        char *buffer;
    } string;
} DP_SaveOraContext;

static DP_SaveOraLayer *save_ora_context_layer_insert(DP_SaveOraContext *c,
                                                      int layer_id, int index)
{
    DP_SaveOraLayer *sol = DP_malloc(sizeof(*sol));
    sol->layer_id = layer_id;
    sol->index = index;
    sol->offset_x = 0;
    sol->offset_y = 0;
    HASH_ADD_INT(c->layers, layer_id, sol);
    return sol;
}

static int save_ora_context_index_get(DP_SaveOraContext *c, int layer_id)
{
    DP_SaveOraLayer *sol;
    HASH_FIND_INT(c->layers, &layer_id, sol);
    if (sol) {
        return sol->index;
    }
    else {
        DP_warn("Layer id %d not found in save index", layer_id);
        return -1;
    }
}

static void save_ora_context_offsets_get(DP_SaveOraContext *c, int layer_id,
                                         int *out_offset_x, int *out_offset_y)
{
    DP_SaveOraLayer *sol;
    HASH_FIND_INT(c->layers, &layer_id, sol);
    if (sol) {
        *out_offset_x = sol->offset_x;
        *out_offset_y = sol->offset_y;
    }
    else {
        *out_offset_x = 0;
        *out_offset_y = 0;
    }
}

static const char *save_ora_context_format(DP_SaveOraContext *c,
                                           const char *fmt, ...)
    DP_FORMAT(2, 3);

static const char *save_ora_context_format(DP_SaveOraContext *c,
                                           const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(c->string.buffer, c->string.capacity, fmt, ap);
    va_end(ap);

    size_t size = DP_int_to_size(len + 1);
    if (size > c->string.capacity) {
        c->string.buffer = DP_realloc(c->string.buffer, size);
        c->string.capacity = size;
        va_start(ap, fmt);
        vsnprintf(c->string.buffer, c->string.capacity, fmt, ap);
        va_end(ap);
    }

    return c->string.buffer;
}

static void save_ora_context_dispose(DP_SaveOraContext *c)
{
    DP_free(c->string.buffer);
    DP_SaveOraLayer *sol, *tmp;
    HASH_ITER(hh, c->layers, sol, tmp) {
        HASH_DEL(c->layers, sol);
        DP_free(sol);
    }
}


static bool ora_store_mimetype(DP_ZipWriter *zw)
{
    const char *mimetype = "image/openraster";
    return DP_zip_writer_add_file(zw, "mimetype", mimetype, strlen(mimetype),
                                  false, false);
}

static bool ora_store_png(DP_SaveOraContext *c, const char *name,
                          bool (*write_png)(void *, DP_Output *), void *user)
{
    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *output = DP_mem_output_new(64, false, &buffer_ptr, &size_ptr);
    bool ok = write_png(user, output);
    void *buffer = *buffer_ptr;
    size_t size = *size_ptr;
    DP_output_free(output);

    if (ok) {
        return DP_zip_writer_add_file(c->zw, name, buffer, size, false, true);
    }
    else {
        DP_free(buffer);
        return false;
    }
}

struct DP_OraWriteUpixelsParams {
    DP_UPixel8 *pixels;
    int width;
    int height;
};

static bool ora_write_png_upixels(void *user, DP_Output *output)
{
    struct DP_OraWriteUpixelsParams *params = user;
    return DP_image_png_write_unpremultiplied(output, params->width,
                                              params->height, params->pixels);
}

static bool ora_store_png_upixels(DP_SaveOraContext *c, DP_UPixel8 *pixels,
                                  int width, int height, const char *name)
{
    static DP_UPixel8 null_pixels[] = {{0}};
    struct DP_OraWriteUpixelsParams params =
        pixels ? (struct DP_OraWriteUpixelsParams){pixels, width, height}
               : (struct DP_OraWriteUpixelsParams){null_pixels, 1, 1};
    return ora_store_png(c, name, ora_write_png_upixels, &params);
}

static bool ora_write_png_image(void *user, DP_Output *output)
{
    DP_Image *img = user;
    return DP_image_write_png(img, output);
}

static bool ora_store_png_image(DP_SaveOraContext *c, DP_Image *img,
                                const char *name)
{
    return img ? ora_store_png(c, name, ora_write_png_image, img)
               : ora_store_png_upixels(c, NULL, 0, 0, name);
}

static bool ora_store_layer(DP_SaveOraContext *c, DP_LayerContent *lc,
                            DP_SaveOraLayer *sol)
{
    int width, height;
    DP_UPixel8 *pixels = DP_layer_content_to_upixels8_cropped(
        lc, false, &sol->offset_x, &sol->offset_y, &width, &height);
    const char *name =
        save_ora_context_format(c, "data/layer-%04x.png", sol->layer_id);
    bool ok = ora_store_png_upixels(c, pixels, width, height, name);
    DP_free(pixels);
    return ok;
}

static bool ora_store_layers(DP_SaveOraContext *c, int *next_index,
                             DP_LayerList *ll, DP_LayerPropsList *lpl)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = count - 1; i >= 0; --i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_SaveOraLayer *sol = save_ora_context_layer_insert(
            c, DP_layer_props_id(lp), (*next_index)++);

        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            DP_LayerList *child_ll = DP_layer_group_children_noinc(lg);
            DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
            if (!ora_store_layers(c, next_index, child_ll, child_lpl)) {
                return false;
            }
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            if (!ora_store_layer(c, lc, sol)) {
                return false;
            }
        }
    }
    return true;
}

static bool ora_store_background(DP_SaveOraContext *c, DP_CanvasState *cs)
{
    DP_Tile *t = DP_canvas_state_background_tile_noinc(cs);
    if (t && !DP_tile_blank(t)) {
        DP_UPixel8 *tile_pixels =
            DP_malloc(sizeof(*tile_pixels) * DP_TILE_LENGTH);
        DP_pixels15_to_8_unpremultiply(tile_pixels, DP_tile_pixels(t),
                                       DP_TILE_LENGTH);
        if (!ora_store_png_upixels(c, tile_pixels, DP_TILE_SIZE, DP_TILE_SIZE,
                                   "data/background-tile.png")) {
            DP_free(tile_pixels);
            return false;
        }

        int width = DP_max_int(1, DP_canvas_state_width(cs));
        int height = DP_max_int(1, DP_canvas_state_height(cs));
        DP_UPixel8 *pixels = DP_malloc(sizeof(*pixels) * DP_int_to_size(width)
                                       * DP_int_to_size(height));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int tx = x % DP_TILE_SIZE;
                int ty = y % DP_TILE_SIZE;
                pixels[y * width + x] = tile_pixels[ty * DP_TILE_SIZE + tx];
            }
        }
        DP_free(tile_pixels);

        bool ok = ora_store_png_upixels(c, pixels, width, height,
                                        "data/background.png");
        DP_free(pixels);
        return ok;
    }
    return true;
}

static bool ora_store_merged(DP_SaveOraContext *c, DP_CanvasState *cs,
                             DP_DrawContext *dc)
{
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    if (!img) {
        return false;
    }

    if (!ora_store_png_image(c, img, "mergedimage.png")) {
        DP_image_free(img);
        return false;
    }

    DP_Image *thumb;
    if (!DP_image_thumbnail(img, dc, 256, 256, &thumb)) {
        DP_image_free(img);
        return false;
    }

    bool ok =
        ora_store_png_image(c, thumb ? thumb : img, "Thumbnails/thumbnail.png");
    DP_free(thumb);
    DP_free(img);
    return ok;
}

#define ORA_APPEND_ATTR(C, OUTPUT, KEY, ...)                            \
    do {                                                                \
        DP_Output *_output = (OUTPUT);                                  \
        DP_OUTPUT_PRINT_LITERAL(_output, " " KEY "=\"");                \
        const char *_value = save_ora_context_format((C), __VA_ARGS__); \
        size_t _len = strlen(_value);                                   \
        for (size_t _i = 0; _i < _len; ++_i) {                          \
            switch (_value[_i]) {                                       \
            case '<':                                                   \
                DP_OUTPUT_PRINT_LITERAL(_output, "&lt;");               \
                break;                                                  \
            case '>':                                                   \
                DP_OUTPUT_PRINT_LITERAL(_output, "&gt;");               \
                break;                                                  \
            case '&':                                                   \
                DP_OUTPUT_PRINT_LITERAL(_output, "&amp;");              \
                break;                                                  \
            case '"':                                                   \
                DP_OUTPUT_PRINT_LITERAL(_output, "&quot;");             \
                break;                                                  \
            case '\'':                                                  \
                DP_OUTPUT_PRINT_LITERAL(_output, "&apos;");             \
                break;                                                  \
            default:                                                    \
                DP_output_write(_output, &_value[_i], 1);               \
                break;                                                  \
            }                                                           \
        }                                                               \
        DP_OUTPUT_PRINT_LITERAL(_output, "\"");                         \
    } while (0)

static void ora_write_name_attr(DP_SaveOraContext *c, DP_Output *output,
                                const char *value)
{
    if (value && value[0] != '\0') {
        ORA_APPEND_ATTR(c, output, "name", "%s", value);
    }
}

static void ora_write_layer_props_xml(DP_SaveOraContext *c, DP_Output *output,
                                      DP_LayerProps *lp)
{
    double opacity =
        DP_uint16_to_double(DP_layer_props_opacity(lp)) / (double)DP_BIT15;
    ORA_APPEND_ATTR(c, output, "opacity", "%.4f", opacity);

    ora_write_name_attr(c, output, DP_layer_props_title(lp, NULL));

    if (DP_layer_props_hidden(lp)) {
        DP_OUTPUT_PRINT_LITERAL(output, " visibility=\"hidden\"");
    }

    int blend_mode = DP_layer_props_blend_mode(lp);
    if (blend_mode != DP_BLEND_MODE_NORMAL) {
        ORA_APPEND_ATTR(c, output, "composite-op", "%s",
                        DP_blend_mode_svg_name(blend_mode));
    }

    // Drawpile doesn't itself need the alpha preserve property, since its alpha
    // preserve behavior depends on the blend mode, but other software cares.
    // The Recolor blend mode is saved as src-atop, which is alpha-preserving
    // per its definition, so it doesn't get the extra attribute.
    if (DP_blend_mode_preserves_alpha(blend_mode)
        && blend_mode != DP_BLEND_MODE_RECOLOR) {
        DP_OUTPUT_PRINT_LITERAL(output, " alpha-preserve=\"true\"");
    }

    if (DP_layer_props_censored(lp)) {
        DP_OUTPUT_PRINT_LITERAL(output, " drawpile:censored=\"true\"");
    }
}

static void ora_write_layers_xml(DP_SaveOraContext *c, DP_Output *output,
                                 DP_LayerList *ll, DP_LayerPropsList *lpl)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = count - 1; i >= 0; --i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_OUTPUT_PRINT_LITERAL(output, "<stack");
            ora_write_layer_props_xml(c, output, lp);
            if (DP_layer_props_isolated(lp)) {
                DP_OUTPUT_PRINT_LITERAL(output, " isolation=\"isolate\"");
            }
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            DP_LayerList *child_ll = DP_layer_group_children_noinc(lg);
            if (DP_layer_list_count(child_ll) == 0) {
                DP_OUTPUT_PRINT_LITERAL(output, "/>");
            }
            else {
                DP_OUTPUT_PRINT_LITERAL(output, ">");
                ora_write_layers_xml(c, output, child_ll,
                                     DP_layer_props_children_noinc(lp));
                DP_OUTPUT_PRINT_LITERAL(output, "</stack>");
            }
        }
        else {
            DP_OUTPUT_PRINT_LITERAL(output, "<layer");
            int layer_id = DP_layer_props_id(lp);
            ORA_APPEND_ATTR(c, output, "src", "data/layer-%04x.png", layer_id);
            int offset_x, offset_y;
            save_ora_context_offsets_get(c, layer_id, &offset_x, &offset_y);
            if (offset_x != 0) {
                ORA_APPEND_ATTR(c, output, "x", "%d", offset_x);
            }
            if (offset_y != 0) {
                ORA_APPEND_ATTR(c, output, "y", "%d", offset_y);
            }
            ora_write_layer_props_xml(c, output, lp);
            DP_OUTPUT_PRINT_LITERAL(output, "/>");
        }
    }
}

static void ora_write_background_xml(DP_Output *output, DP_Tile *t)
{
    if (t && !DP_tile_blank(t)) {
        DP_OUTPUT_PRINT_LITERAL(
            output, "<layer name=\"Background\" src=\"data/background.png\" "
                    "mypaint:background-tile=\"data/background-tile.png\"/>");
    }
}

static const char *ora_annotation_valign_name(int valign)
{
    switch (valign) {
    case DP_ANNOTATION_VALIGN_TOP:
        return "top";
    case DP_ANNOTATION_VALIGN_CENTER:
        return "center";
    case DP_ANNOTATION_VALIGN_BOTTOM:
        return "bottom";
    default:
        DP_warn("Unknown annotation valign %d", valign);
        return NULL;
    }
}

static void ora_write_cdata_content(DP_Output *output, const char *text)
{
    const char *start = text;
    const char *p;
    while ((p = strstr(start, "]]>")) != NULL) {
        size_t offset = (size_t)(p - start + 2);
        DP_output_write(output, start, offset);
        DP_OUTPUT_PRINT_LITERAL(output, "]]><![CDATA[");
        start += offset;
    }
    DP_output_print(output, start);
}

static void ora_write_annotations_xml(DP_SaveOraContext *c, DP_Output *output,
                                      DP_AnnotationList *al)
{
    int count = DP_annotation_list_count(al);
    if (count != 0) {
        DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:annotations>");
        for (int i = 0; i < count; ++i) {
            DP_Annotation *a = DP_annotation_list_at_noinc(al, i);
            DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:a");
            ORA_APPEND_ATTR(c, output, "x", "%d", DP_annotation_x(a));
            ORA_APPEND_ATTR(c, output, "y", "%d", DP_annotation_y(a));
            ORA_APPEND_ATTR(c, output, "w", "%d", DP_annotation_width(a));
            ORA_APPEND_ATTR(c, output, "h", "%d", DP_annotation_height(a));
            ORA_APPEND_ATTR(
                c, output, "bg", "#%x",
                DP_uint32_to_uint(DP_annotation_background_color(a)));

            const char *valign =
                ora_annotation_valign_name(DP_annotation_valign(a));
            if (valign) {
                ORA_APPEND_ATTR(c, output, "valign", "%s", valign);
            }

            DP_OUTPUT_PRINT_LITERAL(output, "><![CDATA[");
            ora_write_cdata_content(output, DP_annotation_text(a, NULL));
            DP_OUTPUT_PRINT_LITERAL(output, "]]></drawpile:a>");
        }
        DP_OUTPUT_PRINT_LITERAL(output, "</drawpile:annotations>");
    }
}

static void ora_write_key_frame_layer_xml(DP_SaveOraContext *c,
                                          DP_Output *output,
                                          const DP_KeyFrameLayer *kfl)
{
    int index = save_ora_context_index_get(c, kfl->layer_id);
    if (index != -1) {
        DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:keyframelayer");
        ORA_APPEND_ATTR(c, output, "layer", "%d", index);
        if (DP_key_frame_layer_hidden(kfl)) {
            ORA_APPEND_ATTR(c, output, "hidden", "true");
        }
        if (DP_key_frame_layer_revealed(kfl)) {
            ORA_APPEND_ATTR(c, output, "revealed", "true");
        }
        DP_OUTPUT_PRINT_LITERAL(output, "/>");
    }
}

static void ora_write_key_frame_xml(DP_SaveOraContext *c, DP_Output *output,
                                    int frame, DP_KeyFrame *kf)
{
    DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:keyframe");
    ORA_APPEND_ATTR(c, output, "frame", "%d", frame);
    int layer_id = DP_key_frame_layer_id(kf);
    int index = layer_id == 0 ? -1 : save_ora_context_index_get(c, layer_id);
    if (index != -1) {
        ORA_APPEND_ATTR(c, output, "layer", "%d", index);
    }
    ora_write_name_attr(c, output, DP_key_frame_title(kf, NULL));

    int layer_count;
    const DP_KeyFrameLayer *kfls = DP_key_frame_layers(kf, &layer_count);
    if (layer_count > 0) {
        DP_OUTPUT_PRINT_LITERAL(output, ">");
        for (int i = 0; i < layer_count; ++i) {
            ora_write_key_frame_layer_xml(c, output, &kfls[i]);
        }
        DP_OUTPUT_PRINT_LITERAL(output, "</drawpile:keyframe>");
    }
    else {
        DP_OUTPUT_PRINT_LITERAL(output, "/>");
    }
}

static void ora_write_track_xml(DP_SaveOraContext *c, DP_Output *output,
                                DP_Track *t)
{
    DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:track");
    ora_write_name_attr(c, output, DP_track_title(t, NULL));
    int key_frame_count = DP_track_key_frame_count(t);
    if (key_frame_count > 0) {
        DP_OUTPUT_PRINT_LITERAL(output, ">");
        for (int i = 0; i < key_frame_count; ++i) {
            ora_write_key_frame_xml(c, output,
                                    DP_track_frame_index_at_noinc(t, i),
                                    DP_track_key_frame_at_noinc(t, i));
        }
        DP_OUTPUT_PRINT_LITERAL(output, "</drawpile:track>");
    }
    else {
        DP_OUTPUT_PRINT_LITERAL(output, "/>");
    }
}

static void ora_write_timeline_xml(DP_SaveOraContext *c, DP_Output *output,
                                   DP_CanvasState *cs)
{
    DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);
    int frame_count = DP_document_metadata_frame_count(dm);
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);
    if (frame_count > 0 && track_count > 0) {
        DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:timeline");
        ORA_APPEND_ATTR(c, output, "frames", "%d", frame_count);
        DP_OUTPUT_PRINT_LITERAL(output, ">");
        for (int i = 0; i < track_count; ++i) {
            ora_write_track_xml(c, output, DP_timeline_at_noinc(tl, i));
        }
        DP_OUTPUT_PRINT_LITERAL(output, "</drawpile:timeline>");
    }
}

static char *ora_write_xml(DP_SaveOraContext *c, DP_CanvasState *cs,
                           size_t *out_size)
{
    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *output = DP_mem_output_new(1024, false, &buffer_ptr, &size_ptr);

    DP_OUTPUT_PRINT_LITERAL(
        output, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<image xmlns:drawpile=\"http://drawpile.net/\" "
                "xmlns:mypaint=\"http://mypaint.org/ns/openraster\"");
    ORA_APPEND_ATTR(c, output, "w", "%d", DP_canvas_state_width(cs));
    ORA_APPEND_ATTR(c, output, "h", "%d", DP_canvas_state_height(cs));
    ORA_APPEND_ATTR(c, output, "version", "0.0.6");
    DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);
    ORA_APPEND_ATTR(c, output, "xres", "%d", DP_document_metadata_dpix(dm));
    ORA_APPEND_ATTR(c, output, "yres", "%d", DP_document_metadata_dpiy(dm));
    ORA_APPEND_ATTR(c, output, "drawpile:framerate", "%d",
                    DP_document_metadata_framerate(dm));
    DP_OUTPUT_PRINT_LITERAL(output, ">");

    DP_OUTPUT_PRINT_LITERAL(output, "<stack>");
    ora_write_layers_xml(c, output, DP_canvas_state_layers_noinc(cs),
                         DP_canvas_state_layer_props_noinc(cs));
    ora_write_background_xml(output, DP_canvas_state_background_tile_noinc(cs));
    DP_OUTPUT_PRINT_LITERAL(output, "</stack>");

    ora_write_annotations_xml(c, output, DP_canvas_state_annotations_noinc(cs));
    ora_write_timeline_xml(c, output, cs);

    DP_OUTPUT_PRINT_LITERAL(output, "</image>");

    *out_size = *size_ptr;
    char *buffer = *buffer_ptr;
    DP_output_free(output);
    return buffer;
}

static bool ora_store_xml(DP_SaveOraContext *c, DP_CanvasState *cs)
{
    size_t size;
    char *buffer = ora_write_xml(c, cs, &size);
    return DP_zip_writer_add_file(c->zw, "stack.xml", buffer, size, true, true);
}

static DP_SaveResult save_ora(DP_CanvasState *cs, const char *path,
                              DP_DrawContext *dc)
{
    DP_ZipWriter *zw = DP_zip_writer_new(path);
    if (!zw) {
        DP_warn("Save '%s': %s", path, DP_error());
    }

    bool base_ok = ora_store_mimetype(zw) && DP_zip_writer_add_dir(zw, "data")
                && DP_zip_writer_add_dir(zw, "Thumbnails");
    if (!base_ok) {
        DP_warn("Save '%s': %s", path, DP_error());
        DP_zip_writer_free_abort(zw);
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    DP_SaveOraContext c = {zw, NULL, {0, NULL}};
    int next_index = 0;
    bool content_ok =
        ora_store_layers(&c, &next_index, DP_canvas_state_layers_noinc(cs),
                         DP_canvas_state_layer_props_noinc(cs))
        && ora_store_background(&c, cs) && ora_store_merged(&c, cs, dc)
        && ora_store_xml(&c, cs);
    save_ora_context_dispose(&c);
    if (!content_ok) {
        DP_warn("Save '%s': %s", path, DP_error());
        DP_zip_writer_free_abort(zw);
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    if (!DP_zip_writer_free_finish(zw)) {
        DP_warn("Save '%s': %s", path, DP_error());
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    return DP_SAVE_RESULT_SUCCESS;
}


static DP_SaveResult save_png(DP_Image *img, DP_Output *output)
{
    bool ok = DP_image_write_png(img, output);
    if (ok) {
        return DP_SAVE_RESULT_SUCCESS;
    }
    else {
        DP_warn("Save PNG: %s", DP_error());
        return DP_SAVE_RESULT_WRITE_ERROR;
    }
}

static DP_SaveResult save_jpeg(DP_Image *img, DP_Output *output)
{
    bool ok = DP_image_write_jpeg(img, output);
    if (ok) {
        return DP_SAVE_RESULT_SUCCESS;
    }
    else {
        DP_warn("Save JPEG: %s", DP_error());
        return DP_SAVE_RESULT_WRITE_ERROR;
    }
}

static void blend_annotation(DP_Pixel8 *restrict dst, DP_Rect dst_rect,
                             const DP_Pixel8 *restrict src, DP_Rect src_rect)
{
    int dst_width = DP_rect_width(dst_rect);
    int src_x = DP_rect_x(src_rect);
    int src_y = DP_rect_y(src_rect);
    int src_width = DP_rect_width(src_rect);
    DP_Rect rect = DP_rect_intersection(dst_rect, src_rect);
    int stride = DP_rect_width(rect);
    for (int y = rect.y1; y <= rect.y2; ++y) {
        DP_blend_pixels8(dst + dst_width * y + rect.x1,
                         src + (y - src_y) * src_width + (rect.x1 - src_x),
                         stride, 255);
    }
}

static void bake_annotations(DP_AnnotationList *al, DP_DrawContext *dc,
                             DP_Image *img,
                             DP_SaveBakeAnnotationFn bake_annotation,
                             void *user)
{
    int annotation_count = DP_annotation_list_count(al);
    DP_Rect img_rect =
        DP_rect_make(0, 0, DP_image_width(img), DP_image_height(img));
    for (int i = 0; i < annotation_count; ++i) {
        DP_Annotation *a = DP_annotation_list_at_noinc(al, i);
        int width = DP_annotation_width(a);
        int height = DP_annotation_height(a);
        DP_Rect annotation_rect =
            DP_rect_make(DP_annotation_x(a), DP_annotation_y(a), width, height);
        if (DP_rect_intersects(img_rect, annotation_rect)) {
            size_t size = DP_int_to_size(width) * DP_int_to_size(height)
                        * sizeof(DP_Pixel8);
            void *buffer = DP_draw_context_pool_require(dc, size);
            if (bake_annotation(user, a, buffer)) {
                blend_annotation(DP_image_pixels(img), img_rect, buffer,
                                 annotation_rect);
            }
        }
    }
}

static DP_SaveResult save_flat_image(
    DP_CanvasState *cs, DP_DrawContext *dc, DP_Rect *crop, const char *path,
    DP_SaveResult (*save_fn)(DP_Image *, DP_Output *), DP_ViewModeFilter vmf,
    DP_SaveBakeAnnotationFn bake_annotation, void *user)
{
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, crop, &vmf);
    if (!img) {
        DP_warn("Save: %s", DP_error());
        return DP_SAVE_RESULT_FLATTEN_ERROR;
    }

    if (bake_annotation) {
        DP_ASSERT(dc);
        DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
        bake_annotations(al, dc, img, bake_annotation, user);
    }

    DP_Output *output = DP_file_output_save_new_from_path(path);
    if (!output) {
        DP_warn("Save: %s", DP_error());
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    DP_SaveResult result = save_fn(img, output);
    DP_output_free(output);
    DP_image_free(img);
    return result;
}


static DP_SaveResult save(DP_CanvasState *cs, DP_DrawContext *dc,
                          DP_SaveImageType type, const char *path,
                          DP_SaveBakeAnnotationFn bake_annotation, void *user)
{
    switch (type) {
    case DP_SAVE_IMAGE_ORA:
        return save_ora(cs, path, dc);
    case DP_SAVE_IMAGE_PNG:
        return save_flat_image(cs, dc, NULL, path, save_png,
                               DP_view_mode_filter_make_default(),
                               bake_annotation, user);
    case DP_SAVE_IMAGE_JPEG:
        return save_flat_image(cs, dc, NULL, path, save_jpeg,
                               DP_view_mode_filter_make_default(),
                               bake_annotation, user);
    case DP_SAVE_IMAGE_PSD:
        return DP_save_psd(cs, path, dc);
    default:
        DP_error_set("Unknown save format");
        return DP_SAVE_RESULT_UNKNOWN_FORMAT;
    }
}

DP_SaveResult DP_save(DP_CanvasState *cs, DP_DrawContext *dc,
                      DP_SaveImageType type, const char *path,
                      DP_SaveBakeAnnotationFn bake_annotation, void *user)
{
    if (cs && path) {
        DP_PERF_BEGIN_DETAIL(fn, "image", "path=%s", path);
        DP_SaveResult result = save(cs, dc, type, path, bake_annotation, user);
        DP_PERF_END(fn);
        return result;
    }
    else {
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }
}


#if defined(_WIN32)
#    define PREFERRED_PATH_SEPARATOR "\\"
#    define POSSIBLE_PATH_SEPARATORS "\\/"
#elif defined(__EMSCRIPTEN__) || defined(__APPLE__) || defined(__linux__)
#    define PREFERRED_PATH_SEPARATOR "/"
#    define POSSIBLE_PATH_SEPARATORS "/"
#else
#    error "unknown platform"
#endif

static const char *get_path_separator(const char *path)
{
    size_t len = strlen(path);
    char last = len > 0 ? path[len - 1] : '\0';
    size_t possible_path_separators_len = strlen(POSSIBLE_PATH_SEPARATORS);
    for (size_t i = 0; i < possible_path_separators_len; ++i) {
        if (last == POSSIBLE_PATH_SEPARATORS[i]) {
            return "";
        }
    }
    return PREFERRED_PATH_SEPARATOR;
}

struct DP_SaveFrameContext {
    DP_CanvasState *cs;
    DP_ViewModeBuffer *vmbs;
    DP_Rect *crop;
    int frame_count;
    const char *path;
    const char *separator;
    DP_SaveAnimationProgressFn progress_fn;
    void *user;
    DP_Mutex *progress_mutex;
    DP_Atomic result;
    int frames_done;
};

struct DP_SaveFrameJobParams {
    struct DP_SaveFrameContext *c;
    int count;
    int frames[];
};

static void set_error_result(struct DP_SaveFrameContext *c,
                             DP_SaveResult result)
{
    if (result != DP_SAVE_RESULT_SUCCESS) {
        DP_atomic_compare_exchange(&c->result, DP_SAVE_RESULT_SUCCESS,
                                   (int)result);
    }
}

static char *format_frame_path(struct DP_SaveFrameContext *c, int frame_index)
{
    return DP_format("%s%sframe-%03d.png", c->path, c->separator,
                     frame_index + 1);
}

static char *save_frame(struct DP_SaveFrameContext *c, DP_ViewModeBuffer *vmb,
                        int frame_index)
{
    DP_CanvasState *cs = c->cs;
    char *path = format_frame_path(c, frame_index);
    DP_SaveResult result = save_flat_image(
        cs, NULL, c->crop, path, save_png,
        DP_view_mode_filter_make_frame_render(vmb, cs, frame_index), NULL,
        NULL);
    set_error_result(c, result);
    return path;
}

static void copy_frame(struct DP_SaveFrameContext *c, char *source_path,
                       int frame_index)
{
    char *target_path = format_frame_path(c, frame_index);
    if (!DP_file_copy(source_path, target_path)) {
        DP_warn("Error copying frame from '%s' to '%s': %s", source_path,
                target_path, DP_error());
        set_error_result(c, DP_SAVE_RESULT_WRITE_ERROR);
    }
    DP_free(target_path);
}

static int count_frames(int start, int end_inclusive)
{
    return end_inclusive - start + 1;
}

static void report_progress(struct DP_SaveFrameContext *c)
{
    DP_SaveAnimationProgressFn progress_fn = c->progress_fn;
    if (progress_fn && DP_atomic_get(&c->result) == DP_SAVE_RESULT_SUCCESS) {
        DP_Mutex *progress_mutex = c->progress_mutex;
        DP_MUTEX_MUST_LOCK(progress_mutex);
        int done = ++c->frames_done;
        bool keep_going = progress_fn(
            c->user, DP_int_to_double(done) / DP_int_to_double(c->frame_count));
        DP_MUTEX_MUST_UNLOCK(progress_mutex);
        if (!keep_going) {
            set_error_result(c, DP_SAVE_RESULT_CANCEL);
        }
    }
}

static void save_frame_job(void *element, int thread_index)
{
    struct DP_SaveFrameJobParams *params =
        *(struct DP_SaveFrameJobParams **)element;
    struct DP_SaveFrameContext *c = params->c;
    if (DP_atomic_get(&c->result) == DP_SAVE_RESULT_SUCCESS) {
        DP_ViewModeBuffer *vmb = &c->vmbs[thread_index];
        // Render and save the first frame given.
        int first_frame = params->frames[0];
        char *path = save_frame(c, vmb, first_frame);
        report_progress(c);
        // Subsequent frames are the same, so just copy the files.
        int count = params->count;
        for (int i = 1; i < count; ++i) {
            if (DP_atomic_get(&c->result) == DP_SAVE_RESULT_SUCCESS) {
                copy_frame(c, path, params->frames[i]);
                report_progress(c);
            }
        }
        DP_free(path);
    }
    DP_free(params);
}

static DP_SaveResult
save_animation_frames(DP_CanvasState *cs, const char *path,
                      DP_SaveAnimationProgressFn progress_fn, void *user,
                      DP_Rect *crop, int start, int end_inclusive)
{
    if (end_inclusive < start) {
        return DP_SAVE_RESULT_SUCCESS;
    }

    DP_Mutex *progress_mutex;
    if (progress_fn) {
        progress_mutex = DP_mutex_new();
        if (!progress_mutex) {
            return DP_SAVE_RESULT_INTERNAL_ERROR;
        }
    }
    else {
        progress_mutex = NULL;
    }

    int frame_count = count_frames(start, end_inclusive);
    DP_Worker *worker = DP_worker_new(DP_int_to_size(frame_count),
                                      sizeof(struct DP_SaveFrameJobParams *),
                                      DP_worker_cpu_count(128), save_frame_job);
    if (!worker) {
        DP_mutex_free(progress_mutex);
        return DP_SAVE_RESULT_INTERNAL_ERROR;
    }

    int thread_count = DP_worker_thread_count(worker);
    struct DP_SaveFrameContext c = {
        cs,
        DP_malloc(sizeof(*c.vmbs) * DP_int_to_size(thread_count)),
        crop,
        frame_count,
        path,
        get_path_separator(path),
        progress_fn,
        user,
        progress_mutex,
        DP_ATOMIC_INIT(DP_SAVE_RESULT_SUCCESS),
        0,
    };

    for (int i = 0; i < thread_count; ++i) {
        DP_view_mode_buffer_init(&c.vmbs[i]);
    }

    int *frames = DP_malloc(sizeof(*frames) * DP_int_to_size(frame_count));
    for (int i = 0; i < frame_count; ++i) {
        frames[i] = start + i;
    }

    int frames_left = frame_count;
    while (frames_left != 0) {
        // Collect same frames into a single job by swapping them to the front.
        int first_frame = frames[0];
        int count = 1;
        for (int i = 1; i < frames_left; ++i) {
            int frame = frames[i];
            if (DP_canvas_state_same_frame(cs, first_frame, frame)) {
                frames[i] = frames[count];
                frames[count] = frame;
                ++count;
            }
        }

        size_t scount = DP_int_to_size(count);
        size_t size = scount * sizeof(*frames);
        struct DP_SaveFrameJobParams *params = DP_malloc(
            DP_FLEX_SIZEOF(struct DP_SaveFrameJobParams, frames, scount));
        params->c = &c;
        params->count = count;
        memcpy(params->frames, frames, size);
        DP_worker_push(worker, &params);

        frames_left -= count;
        memmove(frames, frames + count,
                DP_int_to_size(frames_left) * sizeof(*frames));
    }

    DP_free(frames);
    DP_worker_free_join(worker);
    DP_mutex_free(progress_mutex);

    for (int i = 0; i < thread_count; ++i) {
        DP_view_mode_buffer_dispose(&c.vmbs[i]);
    }
    DP_free(c.vmbs);

    return (DP_SaveResult)DP_atomic_get(&c.result);
}

DP_SaveResult DP_save_animation_frames(DP_CanvasState *cs, const char *path,
                                       DP_Rect *crop, int start,
                                       int end_inclusive,
                                       DP_SaveAnimationProgressFn progress_fn,
                                       void *user)
{
    if (cs && path) {
        int frame_count = DP_canvas_state_frame_count(cs);
        if (start < 0) {
            start = 0;
        }
        if (end_inclusive < 0) {
            end_inclusive = frame_count - 1;
        }

        DP_PERF_BEGIN_DETAIL(fn, "animation_frames", "frame_count=%d,path=%s",
                             count_frames(start, end_inclusive), path);
        DP_SaveResult result = save_animation_frames(
            cs, path, progress_fn, user, crop, start, end_inclusive);
        DP_PERF_END(fn);
        return result;
    }
    else {
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }
}


static bool get_gif_dimensions(DP_CanvasState *cs, DP_Rect *crop,
                               int *out_width, int *out_height)
{
    DP_Rect rect = DP_rect_make(0, 0, DP_canvas_state_width(cs),
                                DP_canvas_state_height(cs));
    if (!DP_rect_valid(rect)) {
        return false;
    }

    if (crop) {
        rect = DP_rect_intersection(rect, *crop);
        if (!DP_rect_valid(rect)) {
            return false;
        }
    }

    int width = DP_rect_width(rect);
    int height = DP_rect_height(rect);
    if (width < 1 || height < 1 || width > UINT16_MAX || height > UINT16_MAX) {
        return false;
    }

    *out_width = width;
    *out_height = height;
    return true;
}

static bool write_gif(void *user, const void *buffer, size_t size)
{
    DP_Output *output = user;
    return DP_output_write(output, buffer, size);
}

static jo_gifx_t *start_gif(DP_CanvasState *cs, DP_Rect *crop,
                            DP_Output *output, int width, int height)
{
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, crop, NULL);
    jo_gifx_t *gif = jo_gifx_start(write_gif, output, DP_int_to_uint16(width),
                                   DP_int_to_uint16(height), 0, 255,
                                   (uint32_t *)DP_image_pixels(img));
    DP_image_free(img);
    return gif;
}

static double get_gif_centiseconds_per_frame(int framerate)
{
    return 100.0 / DP_int_to_double(DP_min_int(DP_max_int(framerate, 1), 100));
}

static bool report_gif_progress(DP_SaveAnimationProgressFn progress_fn,
                                void *user, int frames_done, int frame_count)
{
    double part = DP_int_to_double(frames_done);
    double total = DP_int_to_double(frame_count);
    return !progress_fn || progress_fn(user, part / total);
}

static DP_SaveResult save_animation_gif(DP_CanvasState *cs, const char *path,
                                        DP_SaveAnimationProgressFn progress_fn,
                                        void *user, DP_Rect *crop, int start,
                                        int end_inclusive, int framerate)
{
    int width, height;
    if (!get_gif_dimensions(cs, crop, &width, &height)) {
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    DP_Output *output = DP_file_output_new_from_path(path);
    if (!output) {
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    jo_gifx_t *gif = start_gif(cs, crop, output, width, height);
    if (!gif) {
        DP_output_free(output);
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    int frame_count = count_frames(start, end_inclusive);
    if (!report_gif_progress(progress_fn, user, 0, frame_count)) {
        jo_gifx_abort(gif);
        DP_output_free(output);
        return DP_SAVE_RESULT_CANCEL;
    }

    DP_ViewModeBuffer vmb;
    DP_view_mode_buffer_init(&vmb);
    double centiseconds_per_frame = get_gif_centiseconds_per_frame(framerate);
    double delay_frac = 0.0;
    for (int i = start; i <= end_inclusive; ++i) {
        int instances = 1;
        while (i < end_inclusive && DP_canvas_state_same_frame(cs, i, i + 1)) {
            ++i;
            ++instances;
        }

        DP_ViewModeFilter vmf =
            DP_view_mode_filter_make_frame_render(&vmb, cs, i);
        DP_Image *img = DP_canvas_state_to_flat_image(
            cs, DP_FLAT_IMAGE_RENDER_FLAGS, crop, &vmf);
        double delay = centiseconds_per_frame * DP_int_to_double(instances);
        double delay_floored = floor(delay + delay_frac);
        delay_frac = delay - delay_floored;
        bool frame_ok = jo_gifx_frame(write_gif, output, gif,
                                      (uint32_t *)DP_image_pixels(img),
                                      DP_double_to_uint16(delay_floored));
        DP_image_free(img);
        if (!frame_ok) {
            jo_gifx_abort(gif);
            DP_output_free(output);
            DP_view_mode_buffer_dispose(&vmb);
            return DP_SAVE_RESULT_WRITE_ERROR;
        }

        if (!report_gif_progress(progress_fn, user, i - start + 1,
                                 frame_count)) {
            jo_gifx_abort(gif);
            DP_output_free(output);
            DP_view_mode_buffer_dispose(&vmb);
            return DP_SAVE_RESULT_CANCEL;
        }
    }
    DP_view_mode_buffer_dispose(&vmb);

    if (!jo_gifx_end(write_gif, output, gif) || !DP_output_flush(output)) {
        DP_output_free(output);
        return DP_SAVE_RESULT_WRITE_ERROR;
    }

    DP_output_free(output);
    return DP_SAVE_RESULT_SUCCESS;
}

DP_SaveResult DP_save_animation_gif(DP_CanvasState *cs, const char *path,
                                    DP_Rect *crop, int start, int end_inclusive,
                                    int framerate,
                                    DP_SaveAnimationProgressFn progress_fn,
                                    void *user)
{
    if (cs && path) {
        int frame_count = DP_canvas_state_frame_count(cs);
        if (start < 0) {
            start = 0;
        }
        if (end_inclusive < 0) {
            end_inclusive = frame_count - 1;
        }
        if (framerate < 0) {
            framerate = DP_canvas_state_framerate(cs);
        }

        DP_PERF_BEGIN_DETAIL(fn, "animation_gif", "frame_count=%d,path=%s",
                             count_frames(start, end_inclusive), path);
        DP_SaveResult result = save_animation_gif(
            cs, path, progress_fn, user, crop, start, end_inclusive, framerate);
        DP_PERF_END(fn);
        return result;
    }
    else {
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }
}
