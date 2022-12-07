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
#include "frame.h"
#include "image.h"
#include "image_png.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "tile.h"
#include "timeline.h"
#include "view_mode.h"
#include "zip_archive.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpmsg/blend_mode.h>
#include <ctype.h>
#include <uthash_inc.h>


const DP_SaveFormat *DP_save_supported_formats(void)
{
    static const char *ora_ext[] = {"ora", NULL};
    static const char *png_ext[] = {"png", NULL};
    static const char *jpeg_ext[] = {"jpg", "jpeg", NULL};
    static const DP_SaveFormat formats[] = {
        {"OpenRaster", ora_ext},
        {"PNG", png_ext},
        {"JPEG", jpeg_ext},
        {NULL, NULL},
    };
    return formats;
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
    return sol ? sol->index : -1;
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

static bool ora_store_png(DP_SaveOraContext *c, DP_Image *img_or_null,
                          const char *name)
{
    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *output = DP_mem_output_new(64, false, &buffer_ptr, &size_ptr);

    bool ok = img_or_null
                ? DP_image_write_png(img_or_null, output)
                : DP_image_png_write(output, 1, 1, (DP_Pixel8[]){{0}});

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

static bool ora_store_layer(DP_SaveOraContext *c, DP_LayerContent *lc,
                            DP_SaveOraLayer *sol)
{
    DP_Image *img_or_null =
        DP_layer_content_to_image_cropped(lc, &sol->offset_x, &sol->offset_y);
    const char *name =
        save_ora_context_format(c, "data/layer-%04x.png", sol->layer_id);
    bool ok = ora_store_png(c, img_or_null, name);
    DP_image_free(img_or_null);
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
        DP_Image *tile_img = DP_image_new(DP_TILE_SIZE, DP_TILE_SIZE);
        DP_pixels15_to_8(DP_image_pixels(tile_img), DP_tile_pixels(t),
                         DP_TILE_LENGTH);
        if (!ora_store_png(c, tile_img, "data/background-tile.png")) {
            DP_image_free(tile_img);
            return false;
        }

        int width = DP_max_int(1, DP_canvas_state_width(cs));
        int height = DP_max_int(1, DP_canvas_state_height(cs));
        DP_Image *img = DP_image_new(width, height);

        DP_Pixel8 solid;
        if (DP_image_same_pixel(tile_img, &solid)) {
            DP_Pixel8 *pixels = DP_image_pixels(img);
            int count = width * height;
            for (int i = 0; i < count; ++i) {
                pixels[i] = solid;
            }
        }
        else {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    DP_Pixel8 pixel = DP_image_pixel_at(
                        tile_img, x % DP_TILE_SIZE, y % DP_TILE_SIZE);
                    DP_image_pixel_at_set(img, x, y, pixel);
                }
            }
        }
        DP_image_free(tile_img);

        bool ok = ora_store_png(c, img, "data/background.png");
        DP_image_free(img);
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

    if (!ora_store_png(c, img, "mergedimage.png")) {
        DP_image_free(img);
        return false;
    }

    DP_Image *thumb;
    if (!DP_image_thumbnail(img, dc, 256, 256, &thumb)) {
        DP_image_free(img);
        return false;
    }

    bool ok = ora_store_png(c, thumb ? thumb : img, "Thumbnails/thumbnail.png");
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

static void ora_write_layer_props_xml(DP_SaveOraContext *c, DP_Output *output,
                                      DP_LayerProps *lp)
{
    double opacity =
        DP_uint16_to_double(DP_layer_props_opacity(lp)) / (double)DP_BIT15;
    ORA_APPEND_ATTR(c, output, "opacity", "%.4f", opacity);

    const char *title = DP_layer_props_title(lp, NULL);
    if (title[0] != '\0') {
        ORA_APPEND_ATTR(c, output, "name", "%s", title);
    }

    if (DP_layer_props_hidden(lp)) {
        DP_OUTPUT_PRINT_LITERAL(output, " visibility=\"hidden\"");
    }

    int blend_mode = DP_layer_props_blend_mode(lp);
    if (blend_mode != DP_BLEND_MODE_NORMAL) {
        ORA_APPEND_ATTR(c, output, "composite-op", "%s",
                        DP_blend_mode_svg_name(blend_mode));
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
            DP_OUTPUT_PRINT_LITERAL(output, "><![CDATA[");
            ora_write_cdata_content(output, DP_annotation_text(a, NULL));
            DP_OUTPUT_PRINT_LITERAL(output, "]]></drawpile:a>");
        }
        DP_OUTPUT_PRINT_LITERAL(output, "</drawpile:annotations>");
    }
}

static void ora_write_frame_xml(DP_SaveOraContext *c, DP_Output *output,
                                DP_Frame *f)
{
    bool have_index = false;
    int layer_id_count = DP_frame_layer_id_count(f);
    for (int i = 0; i < layer_id_count; ++i) {
        int layer_id = DP_frame_layer_id_at(f, i);
        int index = save_ora_context_index_get(c, layer_id);
        if (index != -1) {
            if (have_index) {
                DP_output_format(output, " %d", index);
            }
            else {
                have_index = true;
                DP_output_format(output, "<frame>%d", index);
            }
        }
    }
    if (have_index) {
        DP_OUTPUT_PRINT_LITERAL(output, "</frame>");
    }
    else {
        DP_OUTPUT_PRINT_LITERAL(output, "<frame/>");
    }
}

static void ora_write_timeline_xml(DP_SaveOraContext *c, DP_Output *output,
                                   DP_CanvasState *cs, bool use_timeline)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int frame_count = DP_timeline_frame_count(tl);
    if (frame_count != 0) {
        DP_OUTPUT_PRINT_LITERAL(output, "<drawpile:timeline");
        ORA_APPEND_ATTR(c, output, "enabled", "%s",
                        use_timeline ? "true" : "false");
        DP_OUTPUT_PRINT_LITERAL(output, ">");
        for (int i = 0; i < frame_count; ++i) {
            ora_write_frame_xml(c, output, DP_timeline_frame_at_noinc(tl, i));
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
    ORA_APPEND_ATTR(c, output, "version", "0.0.3");
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
    ora_write_timeline_xml(c, output, cs,
                           DP_document_metadata_use_timeline(dm));

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

static DP_SaveResult save_flat_image(DP_CanvasState *cs, const char *path,
                                     DP_SaveResult (*save_fn)(DP_Image *,
                                                              DP_Output *),
                                     DP_ViewModeFilter vmf)
{
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, &vmf);
    if (!img) {
        DP_warn("Save: %s", DP_error());
        return DP_SAVE_RESULT_FLATTEN_ERROR;
    }

    DP_Output *output = DP_file_output_new_from_path(path);
    if (!output) {
        DP_warn("Save: %s", DP_error());
        return DP_SAVE_RESULT_OPEN_ERROR;
    }

    DP_SaveResult result = save_fn(img, output);
    DP_output_free(output);
    DP_image_free(img);
    return result;
}

DP_SaveResult DP_save(DP_CanvasState *cs, DP_DrawContext *dc, const char *path)
{
    if (!cs || !path) {
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    const char *dot = strrchr(path, '.');
    if (!dot) {
        return DP_SAVE_RESULT_NO_EXTENSION;
    }

    const char *ext = dot + 1;
    if (DP_str_equal_lowercase(ext, "ora")) {
        return save_ora(cs, path, dc);
    }
    else if (DP_str_equal_lowercase(ext, "png")) {
        return save_flat_image(cs, path, save_png,
                               DP_view_mode_filter_make_default());
    }
    else if (DP_str_equal_lowercase(ext, "jpg")
             || DP_str_equal_lowercase(ext, "jpeg")) {
        return save_flat_image(cs, path, save_jpeg,
                               DP_view_mode_filter_make_default());
    }
    else {
        return DP_SAVE_RESULT_UNKNOWN_FORMAT;
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
    int frame_index;
};

static void set_error_result(struct DP_SaveFrameContext *c,
                             DP_SaveResult result)
{
    if (result != DP_SAVE_RESULT_SUCCESS) {
        DP_atomic_compare_exchange(&c->result, DP_SAVE_RESULT_SUCCESS,
                                   (int)result);
    }
}

static void save_frame(struct DP_SaveFrameContext *c, int frame_index)
{
    DP_CanvasState *cs = c->cs;
    char *path =
        DP_format("%s%sframe-%03d.png", c->path, c->separator, frame_index + 1);
    DP_SaveResult result = save_flat_image(
        cs, path, save_png, DP_view_mode_filter_make_frame(cs, frame_index));
    set_error_result(c, result);
    DP_free(path);
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

static void save_frame_job(void *element, DP_UNUSED int thread_index)
{
    struct DP_SaveFrameJobParams *params = element;
    struct DP_SaveFrameContext *c = params->c;
    if (DP_atomic_get(&c->result) == DP_SAVE_RESULT_SUCCESS) {
        save_frame(c, params->frame_index);
        report_progress(c);
    }
}

DP_SaveResult DP_save_animation_frames(DP_CanvasState *cs, const char *path,
                                       DP_SaveAnimationProgressFn progress_fn,
                                       void *user)
{
    if (!cs || !path) {
        return DP_SAVE_RESULT_BAD_ARGUMENTS;
    }

    int frame_count = DP_canvas_state_frame_count(cs);
    if (frame_count == 0) {
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

    DP_Worker *worker = DP_worker_new(DP_int_to_size(frame_count),
                                      sizeof(struct DP_SaveFrameJobParams),
                                      DP_thread_cpu_count(), save_frame_job);
    if (!worker) {
        DP_mutex_free(progress_mutex);
        return DP_SAVE_RESULT_INTERNAL_ERROR;
    }

    struct DP_SaveFrameContext c = {
        cs,
        frame_count,
        path,
        get_path_separator(path),
        progress_fn,
        user,
        progress_mutex,
        DP_ATOMIC_INIT(DP_SAVE_RESULT_SUCCESS),
        0,
    };

    for (int i = 0; i < frame_count; ++i) {
        struct DP_SaveFrameJobParams params = {&c, i};
        DP_worker_push(worker, &params);
    }

    DP_worker_free_join(worker);
    DP_mutex_free(progress_mutex);
    return (DP_SaveResult)DP_atomic_get(&c.result);
}
