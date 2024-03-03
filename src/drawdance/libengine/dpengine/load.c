/*
 * Copyright (C) 2022 - 2023 askmeaboutloom
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
#include "load.h"
#include "annotation.h"
#include "canvas_state.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "image.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "ops.h"
#include "player.h"
#include "save.h"
#include "text.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include "xml_stream.h"
#include "zip_archive.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/vector.h>
#include <dpcommon/worker.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/blend_mode.h>
#include <ctype.h>

#define DP_PERF_CONTEXT "load"


#define DRAWPILE_NAMESPACE "http://drawpile.net/"
#define MYPAINT_NAMESPACE  "http://mypaint.org/ns/openraster"
#define START_ID           0x100


const DP_LoadFormat *DP_load_supported_formats(void)
{
    static const char *ora_ext[] = {"ora", NULL};
    static const char *png_ext[] = {"png", NULL};
    static const char *jpeg_ext[] = {"jpg", "jpeg", NULL};
    static const char *psd_ext[] = {"psd", NULL};
    static const DP_LoadFormat formats[] = {
        {"OpenRaster", ora_ext},         {"PNG", png_ext}, {"JPEG", jpeg_ext},
        {"Photoshop Document", psd_ext}, {NULL, NULL},
    };
    return formats;
}


static void assign_load_result(DP_LoadResult *out_result, DP_LoadResult result)
{
    if (out_result) {
        *out_result = result;
    }
}

static void assign_type(DP_SaveImageType *out_type, DP_SaveImageType type)
{
    if (out_type) {
        *out_type = type;
    }
}


typedef enum DP_ReadOraExpect {
    DP_READ_ORA_EXPECT_IMAGE,
    DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE,
    DP_READ_ORA_EXPECT_STACK_OR_LAYER,
    DP_READ_ORA_EXPECT_LAYER_END,
    DP_READ_ORA_EXPECT_IMAGE_END,
    DP_READ_ORA_EXPECT_ANNOTATION,
    DP_READ_ORA_EXPECT_ANNOTATION_CONTENT,
    DP_READ_ORA_EXPECT_TRACK,
    DP_READ_ORA_EXPECT_KEY_FRAME,
    DP_READ_ORA_EXPECT_KEY_FRAME_END,
    DP_READ_ORA_EXPECT_KEY_FRAME_LAYER,
    DP_READ_ORA_EXPECT_KEY_FRAME_LAYER_END,
    DP_READ_ORA_EXPECT_END,
} DP_ReadOraExpect;

typedef struct DP_ReadOraGroup {
    DP_TransientLayerGroup *tlg;
    DP_TransientLayerProps *tlp;
    int child_count;
} DP_ReadOraGroup;

typedef struct DP_ReadOraChildren {
    union {
        DP_TransientLayerContent *tlc;
        DP_TransientLayerGroup *tlg;
    };
    DP_TransientLayerProps *tlp;
} DP_ReadOraChildren;

typedef struct DP_ReadOraKeyFrameLayer {
    int layer_index;
    unsigned int flags;
} DP_ReadOraKeyFrameLayer;

typedef struct DP_ReadOraKeyFrame {
    int frame;
    int layer_index;
    DP_Text *title;
    DP_Vector layers;
} DP_ReadOraKeyFrame;

typedef struct DP_ReadOraTrack {
    int id;
    DP_Text *title;
    DP_Vector key_frames;
} DP_ReadOraTrack;

typedef struct DP_ReadOraContext {
    DP_DrawContext *dc;
    struct {
        DP_LoadFixedLayerFn fn;
        void *user;
    } fixed_layer;
    DP_ZipReader *zr;
    DP_Worker *worker;
    DP_ReadOraExpect expect;
    DP_TransientCanvasState *tcs;
    bool want_layers;
    bool want_annotations;
    bool want_timeline;
    bool intuit_background;
    bool annotations_in_stack;
    int next_layer_id;
    int next_annotation_id;
    int next_track_id;
    int garbage_depth;
    DP_Queue groups;
    DP_Queue children;
    DP_Queue annotations;
    DP_Vector tracks;
    size_t text_capacity;
    size_t text_len;
    char *text;
} DP_ReadOraContext;

static void dispose_child(void *element, DP_UNUSED void *user)
{
    DP_ReadOraChildren *roc = element;
    DP_TransientLayerProps *tlp = roc->tlp;
    if (DP_transient_layer_props_children_noinc(tlp)) {
        DP_transient_layer_group_decref(roc->tlg);
    }
    else {
        DP_transient_layer_content_decref(roc->tlc);
    }
    DP_transient_layer_props_decref(tlp);
}

static void dispose_annotation(void *element, DP_UNUSED void *user)
{
    DP_Annotation *a = element;
    DP_annotation_decref(a);
}

static void dispose_key_frame(void *element)
{
    DP_ReadOraKeyFrame *rokf = element;
    DP_vector_dispose(&rokf->layers);
    DP_text_decref_nullable(rokf->title);
}

static void dispose_track(void *element)
{
    DP_ReadOraTrack *rot = element;
    DP_VECTOR_CLEAR_DISPOSE_TYPE(&rot->key_frames, DP_ReadOraKeyFrame,
                                 dispose_key_frame);
    DP_text_decref_nullable(rot->title);
}

static bool ora_check_mimetype(DP_ZipReader *zr)
{
    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(zr, "mimetype");
    if (!zrf) {
        DP_error_set("No mimetype file found in archive");
        return false;
    }

    size_t size = DP_zip_reader_file_size(zrf);
    const char *content = DP_zip_reader_file_content(zrf);
    // Skip leading whitespace, we allow that kinda thing.
    size_t i = 0;
    while (i < size && isspace(content[i])) {
        ++i;
    }
    // Actually compare the mime type.
    const char *mimetype = "image/openraster";
    size_t len = strlen(mimetype);
    if (size - i < len || memcmp(content + i, mimetype, len) != 0) {
        DP_error_set("Incorrect mime type");
        DP_zip_reader_file_free(zrf);
        return false;
    }
    // Skip trailing whitespace too, especially a trailing newline is likely.
    i += len;
    while (i < size && isspace(content[i])) {
        ++i;
    }
    DP_zip_reader_file_free(zrf);
    if (i == size) {
        return true;
    }
    else {
        DP_error_set("Garbage after mimetype");
        DP_zip_reader_file_free(zrf);
        return false;
    }
}

static bool ora_read_int_attribute(DP_XmlElement *element,
                                   const char *namespace_or_null,
                                   const char *name, long min, long max,
                                   int *out_value)
{
    const char *s = DP_xml_element_attribute(element, namespace_or_null, name);
    if (s) {
        char *end;
        long value = strtol(s, &end, 10);
        if (*end == '\0' && value >= min && value <= max) {
            *out_value = DP_long_to_int(value);
            return true;
        }
    }
    return false;
}

static bool ora_read_float_attribute(DP_XmlElement *element,
                                     const char *namespace_or_null,
                                     const char *name, float min, float max,
                                     float *out_value)
{
    const char *s = DP_xml_element_attribute(element, namespace_or_null, name);
    if (s) {
        char *end;
        float value = strtof(s, &end);
        if (*end == '\0' && value >= min && value <= max) {
            *out_value = value;
            return true;
        }
    }
    return false;
}

static uint32_t ora_read_color_attribute(DP_XmlElement *element,
                                         const char *namespace_or_null,
                                         const char *name)
{
    const char *s = DP_xml_element_attribute(element, namespace_or_null, name);
    if (s && s[0] == '#') {
        size_t len = strlen(s + 1);
        if (len == 6) {
            char *end;
            unsigned long value = strtoul(s + 1, &end, 16);
            if (*end == '\0' && value <= 0xffffffu) {
                return DP_ulong_to_uint32(value) | 0xff000000u;
            }
        }
        else if (len == 8) {
            char *end;
            unsigned long value = strtoul(s + 1, &end, 16);
            if (*end == '\0' && value <= 0xffffffffu) {
                return DP_ulong_to_uint32(value);
            }
        }
    }
    return 0;
}

static void push_group(DP_ReadOraContext *c, DP_TransientLayerGroup *tlg,
                       DP_TransientLayerProps *tlp)
{
    DP_ReadOraGroup *rog = DP_queue_push(&c->groups, sizeof(*rog));
    *rog = (DP_ReadOraGroup){tlg, tlp, 0};
}

static void push_layer_children(DP_ReadOraContext *c, DP_ReadOraChildren roc)
{
    DP_ReadOraGroup *rog = DP_queue_peek_last(&c->groups, sizeof(*rog));
    ++rog->child_count;
    *(DP_ReadOraChildren *)DP_queue_push(&c->children, sizeof(roc)) = roc;
}

static bool ora_handle_image(DP_ReadOraContext *c, DP_XmlElement *element)
{
    // If there's a Drawpile namespace declaration, it's probably one of our own
    // files. That means we shouldn't guess if the bottom layer is a background.
    if (DP_xml_element_contains_namespace_declaration(element,
                                                      DRAWPILE_NAMESPACE)) {
        c->intuit_background = false;
    }

    int width;
    if (!ora_read_int_attribute(element, NULL, "w", 1, INT16_MAX, &width)) {
        DP_error_set("Invalid width");
        return false;
    }

    int height;
    if (!ora_read_int_attribute(element, NULL, "h", 1, INT16_MAX, &height)) {
        DP_error_set("Invalid height");
        return false;
    }

    DP_transient_canvas_state_width_set(c->tcs, width);
    DP_transient_canvas_state_height_set(c->tcs, height);

    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(c->tcs);

    int dpix;
    if (ora_read_int_attribute(element, NULL, "xres", 1, INT32_MAX, &dpix)) {
        DP_transient_document_metadata_dpix_set(tdm, dpix);
    }

    int dpiy;
    if (ora_read_int_attribute(element, NULL, "yres", 1, INT32_MAX, &dpiy)) {
        DP_transient_document_metadata_dpiy_set(tdm, dpiy);
    }

    int framerate;
    if (ora_read_int_attribute(element, DRAWPILE_NAMESPACE, "framerate", 1,
                               INT32_MAX, &framerate)) {
        DP_transient_document_metadata_framerate_set(tdm, framerate);
    }

    push_group(c, NULL, NULL);
    c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
    return true;
}

static int ora_get_next_id(int *next_id)
{
    int id = *next_id;
    if (id <= UINT16_MAX) {
        ++*next_id;
        return id;
    }
    else {
        DP_error_set("Out of ids");
        return -1;
    }
}

static DP_TransientLayerProps *ora_make_layer_props(DP_XmlElement *element,
                                                    int layer_id, bool group)
{
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init(layer_id, group);

    float opacity;
    if (ora_read_float_attribute(element, NULL, "opacity", 0.0f, 1.0f,
                                 &opacity)) {
        DP_transient_layer_props_opacity_set(
            tlp, DP_float_to_uint16(opacity * (float)DP_BIT15 + 0.5f));
    }

    const char *title = DP_xml_element_attribute(element, NULL, "name");
    if (title) {
        DP_transient_layer_props_title_set(tlp, title, strlen(title));
    }

    const char *visibility =
        DP_xml_element_attribute(element, NULL, "visibility");
    if (DP_str_equal_lowercase(visibility, "hidden")) {
        DP_transient_layer_props_hidden_set(tlp, true);
    }

    DP_BlendMode blend_mode = DP_blend_mode_by_svg_name(
        DP_xml_element_attribute(element, NULL, "composite-op"),
        DP_BLEND_MODE_NORMAL);

    // Normal with alpha preserve is Recolor. Drawpile doesn't save it this way,
    // but it's a valid way to represent it, so we handle it.
    if (blend_mode == DP_BLEND_MODE_NORMAL) {
        const char *alpha_preserve =
            DP_xml_element_attribute(element, NULL, "alpha-preserve");
        if (DP_str_equal_lowercase(alpha_preserve, "true")) {
            blend_mode = DP_BLEND_MODE_RECOLOR;
        }
    }

    DP_transient_layer_props_blend_mode_set(tlp, (int)blend_mode);

    const char *censored =
        DP_xml_element_attribute(element, DRAWPILE_NAMESPACE, "censored");
    if (DP_str_equal_lowercase(censored, "true")) {
        DP_transient_layer_props_censored_set(tlp, true);
    }

    return tlp;
}

static void free_zip_reader_file(DP_UNUSED void *buffer, DP_UNUSED size_t size,
                                 void *free_arg)
{
    DP_ZipReaderFile *zrf = free_arg;
    DP_zip_reader_file_free(zrf);
}

static DP_Image *ora_load_png_free(DP_ZipReaderFile *zrf)
{
    DP_Input *input = DP_mem_input_new(DP_zip_reader_file_content(zrf),
                                       DP_zip_reader_file_size(zrf),
                                       free_zip_reader_file, zrf);
    DP_Image *img = DP_image_read_png(input);
    DP_input_free(input);
    return img;
}

static bool ora_try_load_background_tile(DP_ReadOraContext *c,
                                         DP_XmlElement *element)
{
    const char *path =
        DP_xml_element_attribute(element, MYPAINT_NAMESPACE, "background-tile");
    if (!path) {
        return false;
    }

    c->intuit_background = false;

    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, path);
    if (!zrf) {
        DP_warn("Background tile '%s' not found in archive", path);
        return false;
    }

    DP_Image *img = ora_load_png_free(zrf);
    if (!img) {
        DP_warn("Could not read background tile from '%s': %s", path,
                DP_error());
        return false;
    }

    int width = DP_image_width(img);
    int height = DP_image_height(img);
    if (width != DP_TILE_SIZE || height != DP_TILE_SIZE) {
        DP_warn("Invalid background tile dimensions %dx%d", width, height);
        DP_image_free(img);
        return false;
    }

    DP_Tile *tile = DP_tile_new_from_pixels8(0, DP_image_pixels(img));
    DP_transient_canvas_state_background_tile_set_noinc(c->tcs, tile,
                                                        DP_tile_opaque(tile));
    DP_image_free(img);
    return true;
}

struct DP_OraLoadLayerContentParams {
    DP_TransientLayerContent *tlc;
    DP_ZipReaderFile *zrf;
    int x, y;
};

static void ora_load_layer_content_job(void *user, DP_UNUSED int thread_index)
{
    struct DP_OraLoadLayerContentParams *params = user;
    DP_Image *img = ora_load_png_free(params->zrf);
    if (img) {
        DP_transient_layer_content_put_image(
            params->tlc, 1, DP_BLEND_MODE_REPLACE, params->x, params->y, img);
        DP_image_free(img);
    }
    else {
        DP_warn("Error reading ORA layer content: %s", DP_error());
    }
}

static void ora_load_layer_content_in_worker(DP_ReadOraContext *c,
                                             DP_XmlElement *element,
                                             DP_TransientLayerContent *tlc)
{
    const char *src = DP_xml_element_attribute(element, NULL, "src");
    if (!src) {
        return;
    }

    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, src);
    if (!zrf) {
        DP_warn("ORA source '%s' not found in archive", src);
        return;
    }

    struct DP_OraLoadLayerContentParams params = {tlc, zrf, 0, 0};
    ora_read_int_attribute(element, NULL, "x", INT32_MIN, INT32_MAX, &params.x);
    ora_read_int_attribute(element, NULL, "y", INT32_MIN, INT32_MAX, &params.y);
    if (c->worker) {
        DP_worker_push(c->worker, &params);
    }
    else {
        ora_load_layer_content_job(&params, 0);
    }
}

static void ora_handle_fixed_layer(DP_ReadOraContext *c, DP_XmlElement *element,
                                   int layer_id)
{
    // Reporting fixed layers for the Drawpile 2.1 animation import. Those
    // animations don't have an actual timeline, instead each layer is a frame
    // and there's "fixed" layers that stick around for the entire animation.
    DP_LoadFixedLayerFn fixed_layer_fn = c->fixed_layer.fn;
    if (fixed_layer_fn) {
        const char *fixed =
            DP_xml_element_attribute(element, DRAWPILE_NAMESPACE, "fixed");
        if (DP_str_equal_lowercase(fixed, "true")) {
            fixed_layer_fn(c->fixed_layer.user, layer_id);
        }
    }
}

static bool ora_handle_layer(DP_ReadOraContext *c, DP_XmlElement *element)
{
    c->expect = DP_READ_ORA_EXPECT_LAYER_END;
    if (ora_try_load_background_tile(c, element)) {
        return true;
    }

    int layer_id = ora_get_next_id(&c->next_layer_id);
    if (layer_id == -1) {
        return false;
    }

    DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
        DP_transient_canvas_state_width(c->tcs),
        DP_transient_canvas_state_height(c->tcs), NULL);
    // Layer content loading is slow, so we do it asynchronously in a worker.
    // These workers are joined before the canvas state is persisted/freed.
    ora_load_layer_content_in_worker(c, element, tlc);

    DP_TransientLayerProps *tlp =
        ora_make_layer_props(element, layer_id, false);

    push_layer_children(c, (DP_ReadOraChildren){.tlc = tlc, .tlp = tlp});
    ora_handle_fixed_layer(c, element, layer_id);
    return true;
}

static bool ora_handle_stack(DP_ReadOraContext *c, DP_XmlElement *element)
{
    int layer_id = ora_get_next_id(&c->next_layer_id);
    if (layer_id == -1) {
        return false;
    }

    DP_TransientLayerGroup *tlg = DP_transient_layer_group_new_init(
        DP_transient_canvas_state_width(c->tcs),
        DP_transient_canvas_state_height(c->tcs), 0);

    DP_TransientLayerProps *tlp = ora_make_layer_props(element, layer_id, true);
    const char *isolation =
        DP_xml_element_attribute(element, NULL, "isolation");
    if (!DP_str_equal_lowercase(isolation, "isolate")) {
        DP_transient_layer_props_isolated_set(tlp, false);
    }

    push_layer_children(c, (DP_ReadOraChildren){.tlg = tlg, .tlp = tlp});
    push_group(c, tlg, tlp);
    return true;
}

static void ora_handle_stack_end(DP_ReadOraContext *c)
{
    DP_ReadOraGroup *rog = DP_queue_peek_last(&c->groups, sizeof(*rog));
    int count = rog->child_count;

    DP_TransientLayerList *tll;
    DP_TransientLayerPropsList *tlpl;
    if (rog->tlg) {
        DP_ASSERT(rog->tlp);
        tll = DP_transient_layer_group_transient_children(rog->tlg, count);
        tlpl = DP_transient_layer_props_transient_children(rog->tlp, count);
    }
    else {
        DP_ASSERT(!rog->tlp);
        tll = DP_transient_canvas_state_transient_layers(c->tcs, count);
        tlpl = DP_transient_canvas_state_transient_layer_props(c->tcs, count);
        c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
    }

    for (int i = 0; i < count; ++i) {
        DP_ReadOraChildren *roc =
            DP_queue_peek_last(&c->children, sizeof(*roc));
        DP_TransientLayerProps *tlp = roc->tlp;
        if (DP_transient_layer_props_children_noinc(tlp)) {
            DP_transient_layer_list_insert_transient_group_noinc(tll, roc->tlg,
                                                                 i);
        }
        else {
            DP_transient_layer_list_insert_transient_content_noinc(tll,
                                                                   roc->tlc, i);
        }
        DP_transient_layer_props_list_insert_transient_noinc(tlpl, tlp, i);
        DP_queue_pop(&c->children);
    }

    DP_queue_pop(&c->groups);
}

static void ora_handle_annotations(DP_ReadOraContext *c, bool in_stack)
{
    if (c->want_annotations) {
        c->want_annotations = false;
        c->expect = DP_READ_ORA_EXPECT_ANNOTATION;
        c->annotations_in_stack = in_stack;
    }
    else {
        DP_warn("Duplicate <drawpile:annotations>");
        ++c->garbage_depth;
    }
}

static bool ora_handle_annotation(DP_ReadOraContext *c, DP_XmlElement *element)
{
    int annotation_id = ora_get_next_id(&c->next_annotation_id);
    if (annotation_id == -1) {
        return false;
    }

    int x = 0;
    ora_read_int_attribute(element, NULL, "x", INT32_MIN, INT32_MAX, &x);

    int y = 0;
    ora_read_int_attribute(element, NULL, "y", INT32_MIN, INT32_MAX, &y);

    int width = 100;
    ora_read_int_attribute(element, NULL, "w", 1, UINT16_MAX, &width);

    int height = 100;
    ora_read_int_attribute(element, NULL, "h", 1, UINT16_MAX, &height);

    uint32_t background_color = ora_read_color_attribute(element, NULL, "bg");

    DP_TransientAnnotation *ta =
        DP_transient_annotation_new_init(annotation_id, x, y, width, height);
    DP_transient_annotation_background_color_set(ta, background_color);

    DP_TransientAnnotation **pp =
        DP_queue_push(&c->annotations, sizeof(DP_TransientAnnotation *));
    *pp = ta;

    c->text_len = 0;
    c->expect = DP_READ_ORA_EXPECT_ANNOTATION_CONTENT;
    return true;
}

static void ora_handle_annotation_end(DP_ReadOraContext *c)
{
    DP_TransientAnnotation *ta = *(DP_TransientAnnotation **)DP_queue_peek_last(
        &c->annotations, sizeof(DP_TransientAnnotation *));
    DP_transient_annotation_text_set(ta, c->text, c->text_len);
    c->text_len = 0;
    c->text[0] = '\0';
    c->expect = DP_READ_ORA_EXPECT_ANNOTATION;
}

static void ora_handle_annotations_end(DP_ReadOraContext *c)
{
    int count = DP_size_to_int(c->annotations.used);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(c->tcs, count);
    for (int i = 0; i < count; ++i) {
        DP_Annotation *a = *(DP_Annotation **)DP_queue_peek(
            &c->annotations, sizeof(DP_Annotation *));
        DP_transient_annotation_list_insert_noinc(tal, a, i);
        DP_queue_shift(&c->annotations);
    }
    c->expect = c->annotations_in_stack
                  ? DP_READ_ORA_EXPECT_STACK_OR_LAYER
                  : DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
}

static void ora_handle_timeline(DP_ReadOraContext *c, DP_XmlElement *element)
{
    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(c->tcs);

    int frame_count;
    if (ora_read_int_attribute(element, NULL, "frames", 0, INT32_MAX,
                               &frame_count)) {
        DP_transient_document_metadata_frame_count_set(tdm, frame_count);
    }

    DP_VECTOR_INIT_TYPE(&c->tracks, DP_ReadOraTrack, 8);
    c->expect = DP_READ_ORA_EXPECT_TRACK;
}

static void ora_handle_track(DP_ReadOraContext *c, DP_XmlElement *element)
{
    DP_ReadOraTrack rot = {
        ora_get_next_id(&c->next_track_id),
        DP_text_new_nolen(DP_xml_element_attribute(element, NULL, "name")),
        DP_VECTOR_NULL,
    };
    DP_VECTOR_INIT_TYPE(&rot.key_frames, DP_ReadOraKeyFrame, 8);
    DP_VECTOR_PUSH_TYPE(&c->tracks, DP_ReadOraTrack, rot);
    c->expect = DP_READ_ORA_EXPECT_KEY_FRAME;
}

static void ora_handle_key_frame(DP_ReadOraContext *c, DP_XmlElement *element)
{
    DP_DocumentMetadata *dm = DP_transient_canvas_state_metadata_noinc(c->tcs);
    int frame_count = DP_document_metadata_frame_count(dm);

    DP_ReadOraKeyFrame rokf;
    if (!ora_read_int_attribute(element, NULL, "frame", 0, frame_count - 1,
                                &rokf.frame)) {
        DP_warn("Invalid key frame index");
        c->expect = DP_READ_ORA_EXPECT_KEY_FRAME_END;
        return;
    }

    DP_ReadOraTrack *rot = &DP_VECTOR_LAST_TYPE(&c->tracks, DP_ReadOraTrack);
    size_t used = rot->key_frames.used;
    for (size_t i = 0; i < used; ++i) {
        DP_ReadOraKeyFrame existing =
            DP_VECTOR_AT_TYPE(&rot->key_frames, DP_ReadOraKeyFrame, i);
        if (existing.frame == rokf.frame) {
            DP_warn("Duplicate key frame index %d", rokf.frame);
            c->expect = DP_READ_ORA_EXPECT_KEY_FRAME_END;
            return;
        }
    }

    if (!ora_read_int_attribute(element, NULL, "layer", 0, UINT16_MAX,
                                &rokf.layer_index)) {
        rokf.layer_index = -1;
    }

    rokf.title =
        DP_text_new_nolen(DP_xml_element_attribute(element, NULL, "name"));
    DP_VECTOR_INIT_TYPE(&rokf.layers, DP_ReadOraKeyFrameLayer, 8);
    DP_VECTOR_PUSH_TYPE(&rot->key_frames, DP_ReadOraKeyFrame, rokf);
    c->expect = DP_READ_ORA_EXPECT_KEY_FRAME_LAYER;
}

static void ora_handle_key_frame_layer(DP_ReadOraContext *c,
                                       DP_XmlElement *element)
{
    c->expect = DP_READ_ORA_EXPECT_KEY_FRAME_LAYER_END;

    DP_ReadOraKeyFrameLayer rokfl = {0, 0};
    if (!ora_read_int_attribute(element, NULL, "layer", 0, UINT16_MAX,
                                &rokfl.layer_index)) {
        DP_warn("Invalid key frame layer index");
        return;
    }

    const char *hidden = DP_xml_element_attribute(element, NULL, "hidden");
    if (DP_str_equal_lowercase(hidden, "true")) {
        rokfl.flags |= DP_KEY_FRAME_LAYER_HIDDEN;
    }

    const char *revealed = DP_xml_element_attribute(element, NULL, "revealed");
    if (DP_str_equal_lowercase(revealed, "true")) {
        rokfl.flags |= DP_KEY_FRAME_LAYER_REVEALED;
    }

    if (rokfl.flags == 0) {
        DP_warn("Key frame %d has no flags", rokfl.layer_index);
        return;
    }

    DP_ReadOraTrack *rot = &DP_VECTOR_LAST_TYPE(&c->tracks, DP_ReadOraTrack);
    DP_ReadOraKeyFrame *rokf =
        &DP_VECTOR_LAST_TYPE(&rot->key_frames, DP_ReadOraKeyFrame);
    size_t used = rokf->layers.used;
    for (size_t i = 0; i < used; ++i) {
        DP_ReadOraKeyFrameLayer existing =
            DP_VECTOR_AT_TYPE(&rokf->layers, DP_ReadOraKeyFrameLayer, i);
        if (existing.layer_index == rokfl.layer_index) {
            DP_warn("Duplicate key frame layer index %d", rokf->layer_index);
            return;
        }
    }

    DP_VECTOR_PUSH_TYPE(&rokf->layers, DP_ReadOraKeyFrameLayer, rokfl);
}

static int cmp_key_frames(const void *a, const void *b)
{
    int fa = ((const DP_ReadOraKeyFrame *)a)->frame;
    int fb = ((const DP_ReadOraKeyFrame *)b)->frame;
    return fa < fb ? -1 : fa > fb ? 1 : 0;
}

static DP_TransientKeyFrame *ora_fill_key_frame(DP_ReadOraKeyFrame *rokf)
{
    int layer_count = DP_size_to_int(rokf->layers.used);
    int layer_index = rokf->layer_index;
    DP_TransientKeyFrame *tkf = DP_transient_key_frame_new_init(
        layer_index == -1 ? 0 : layer_index + START_ID, layer_count);
    size_t title_length;
    const char *title = DP_text_string(rokf->title, &title_length);
    DP_transient_key_frame_title_set(tkf, title, title_length);
    for (int i = 0; i < layer_count; ++i) {
        DP_ReadOraKeyFrameLayer *rokfl =
            &DP_VECTOR_AT_TYPE(&rokf->layers, DP_ReadOraKeyFrameLayer, i);
        DP_KeyFrameLayer kfl = {rokfl->layer_index + START_ID, rokfl->flags};
        DP_transient_key_frame_layer_set(tkf, kfl, i);
    }
    return tkf;
}

static DP_TransientTrack *ora_fill_track(DP_ReadOraTrack *rot)
{
    DP_VECTOR_SORT_TYPE(&rot->key_frames, DP_ReadOraKeyFrame, cmp_key_frames);
    int key_frame_count = DP_size_to_int(rot->key_frames.used);
    DP_TransientTrack *tt = DP_transient_track_new_init(key_frame_count);
    DP_transient_track_id_set(tt, rot->id);
    size_t title_length;
    const char *title = DP_text_string(rot->title, &title_length);
    DP_transient_track_title_set(tt, title, title_length);
    for (int i = 0; i < key_frame_count; ++i) {
        DP_ReadOraKeyFrame *rokf =
            &DP_VECTOR_AT_TYPE(&rot->key_frames, DP_ReadOraKeyFrame, i);
        DP_TransientKeyFrame *tkf = ora_fill_key_frame(rokf);
        DP_transient_track_set_transient_noinc(tt, rokf->frame, tkf, i);
    }
    return tt;
}

static void ora_fill_timeline(DP_ReadOraContext *c)
{
    int track_count = DP_size_to_int(c->tracks.used);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(c->tcs, track_count);
    for (int i = 0; i < track_count; ++i) {
        DP_ReadOraTrack *rot =
            &DP_VECTOR_AT_TYPE(&c->tracks, DP_ReadOraTrack, i);
        DP_TransientTrack *tt = ora_fill_track(rot);
        DP_transient_timeline_set_transient_noinc(ttl, tt, i);
    }
    DP_transient_canvas_state_timeline_cleanup(c->tcs);
}

static bool ora_xml_start_element(void *user, DP_XmlElement *element)
{
    DP_ReadOraContext *c = user;

    if (c->garbage_depth != 0) {
        ++c->garbage_depth;
        DP_debug("Garbage element <%s:%s> depth %d",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element), c->garbage_depth);
        return true;
    }

    switch (c->expect) {
    case DP_READ_ORA_EXPECT_IMAGE:
        if (DP_xml_element_name_equals(element, NULL, "image")) {
            return ora_handle_image(c, element);
        }
        else {
            DP_debug("Expected <image>, got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
        }
        break;
    case DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE:
        if (DP_xml_element_name_equals(element, NULL, "stack")) {
            if (c->want_layers) {
                c->want_layers = false;
                c->expect = DP_READ_ORA_EXPECT_STACK_OR_LAYER;
            }
            else {
                DP_warn("Duplicate root <stack>");
                ++c->garbage_depth;
            }
        }
        else if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE,
                                            "annotations")) {
            ora_handle_annotations(c, false);
        }
        else if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE,
                                            "timeline")) {
            if (c->want_timeline) {
                c->want_timeline = false;
                ora_handle_timeline(c, element);
            }
            else {
                DP_warn("Duplicate <drawpile:timeline>");
                ++c->garbage_depth;
            }
        }
        else {
            DP_debug("Expected root <stack>, <drawpile:annotations> or "
                     "<drawpile:timeline>, got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
        }
        break;
    case DP_READ_ORA_EXPECT_STACK_OR_LAYER:
        if (DP_xml_element_name_equals(element, NULL, "layer")) {
            return ora_handle_layer(c, element);
        }
        else if (DP_xml_element_name_equals(element, NULL, "stack")) {
            return ora_handle_stack(c, element);
        }
        else if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE,
                                            "annotations")) {
            // Drawpile <2.2 saved annotations inside the layer stack.
            ora_handle_annotations(c, true);
        }
        else {
            DP_debug("Expected <stack>, <layer> or <drawpile:annotations>, got "
                     "<%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
            ++c->garbage_depth;
        }
        break;
    case DP_READ_ORA_EXPECT_LAYER_END:
        DP_debug("Expected </layer>, got <%s:%s>",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element));
        ++c->garbage_depth;
        break;
    case DP_READ_ORA_EXPECT_IMAGE_END:
        DP_debug("Expected </image>, got <%s:%s>",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element));
        ++c->garbage_depth;
        break;
    case DP_READ_ORA_EXPECT_ANNOTATION:
        if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE, "a")) {
            return ora_handle_annotation(c, element);
        }
        else {
            DP_debug("Expected <drawpile:a> got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
            ++c->garbage_depth;
        }
        break;
    case DP_READ_ORA_EXPECT_ANNOTATION_CONTENT:
        DP_debug("Expected text or </drawpile:a>, got <%s:%s>",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element));
        ++c->garbage_depth;
        break;
    case DP_READ_ORA_EXPECT_TRACK:
        if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE, "track")) {
            ora_handle_track(c, element);
        }
        else {
            DP_debug("Expected <drawpile:track> got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
            ++c->garbage_depth;
        }
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME:
        if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE,
                                       "keyframe")) {
            ora_handle_key_frame(c, element);
        }
        else {
            DP_debug("Expected <drawpile:keyframe> got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
            ++c->garbage_depth;
        }
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME_END:
        DP_debug("Expected </drawpile:keyframe> got <%s:%s>",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element));
        ++c->garbage_depth;
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME_LAYER:
        if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE,
                                       "keyframelayer")) {
            ora_handle_key_frame_layer(c, element);
        }
        else {
            DP_debug("Expected <drawpile:keyframelayer> got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
            ++c->garbage_depth;
        }
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME_LAYER_END:
        DP_debug("Expected </drawpile:keyframelayer> got <%s:%s>",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element));
        ++c->garbage_depth;
        break;
    case DP_READ_ORA_EXPECT_END:
        DP_debug("Expected nothing, got <%s:%s>",
                 DP_xml_element_namespace(element),
                 DP_xml_element_name(element));
        break;
    }
    return true;
}

static bool ora_xml_text_content(void *user, size_t len, const char *text)
{
    DP_ReadOraContext *c = user;
    switch (c->expect) {
    case DP_READ_ORA_EXPECT_ANNOTATION_CONTENT: {
        size_t current_len = c->text_len;
        size_t new_len = current_len + len;
        size_t capacity_required = new_len + 1;
        if (c->text_capacity < capacity_required) {
            c->text = DP_realloc(c->text, capacity_required);
        }
        memcpy(c->text + current_len, text, len);
        c->text[new_len] = '\0';
        c->text_len = new_len;
        break;
    }
    default:
        break;
    }
    return true;
}

static bool ora_xml_end_element(void *user)
{
    DP_ReadOraContext *c = user;

    if (c->garbage_depth != 0) {
        --c->garbage_depth;
        DP_debug("Closing garbage element depth %d", c->garbage_depth);
        return true;
    }

    switch (c->expect) {
    case DP_READ_ORA_EXPECT_STACK_OR_LAYER:
        ora_handle_stack_end(c);
        break;
    case DP_READ_ORA_EXPECT_LAYER_END:
        c->expect = DP_READ_ORA_EXPECT_STACK_OR_LAYER;
        break;
    case DP_READ_ORA_EXPECT_IMAGE_END:
        c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
        break;
    case DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE:
        c->expect = DP_READ_ORA_EXPECT_END;
        break;
    case DP_READ_ORA_EXPECT_ANNOTATION:
        ora_handle_annotations_end(c);
        break;
    case DP_READ_ORA_EXPECT_ANNOTATION_CONTENT:
        ora_handle_annotation_end(c);
        break;
    case DP_READ_ORA_EXPECT_TRACK:
        c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME:
        c->expect = DP_READ_ORA_EXPECT_TRACK;
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME_LAYER:
    case DP_READ_ORA_EXPECT_KEY_FRAME_END:
        c->expect = DP_READ_ORA_EXPECT_KEY_FRAME;
        break;
    case DP_READ_ORA_EXPECT_KEY_FRAME_LAYER_END:
        c->expect = DP_READ_ORA_EXPECT_KEY_FRAME_LAYER;
        break;
    case DP_READ_ORA_EXPECT_IMAGE:
    case DP_READ_ORA_EXPECT_END:
        break;
    }
    return true;
}

static DP_CanvasState *ora_read_stack_xml(DP_ReadOraContext *c,
                                          unsigned int flags)
{
    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, "stack.xml");
    if (!zrf) {
        return NULL;
    }

    if ((flags & DP_LOAD_FLAG_SINGLE_THREAD) == 0) {
        c->worker =
            DP_worker_new(64, sizeof(struct DP_OraLoadLayerContentParams),
                          DP_thread_cpu_count(32), ora_load_layer_content_job);
        if (!c->worker) {
            DP_warn("Error creating worker: %s", DP_error());
        }
    }

    c->tcs = DP_transient_canvas_state_new_init();
    bool xml_ok = DP_xml_stream(
        DP_zip_reader_file_size(zrf), DP_zip_reader_file_content(zrf),
        ora_xml_start_element, ora_xml_text_content, ora_xml_end_element, c);
    DP_zip_reader_file_free(zrf);

    if (xml_ok && !c->want_layers) {
        DP_worker_free_join(c->worker);
        if (c->intuit_background) {
            DP_transient_canvas_state_intuit_background(c->tcs);
        }
        DP_transient_canvas_state_layer_routes_reindex(c->tcs, c->dc);
        if (!c->want_timeline) {
            ora_fill_timeline(c);
        }
        return DP_transient_canvas_state_persist(c->tcs);
    }
    else {
        DP_worker_free_join(c->worker);
        DP_transient_canvas_state_decref_nullable(c->tcs);
        return NULL;
    }
}

static DP_CanvasState *load_ora(DP_DrawContext *dc, const char *path,
                                unsigned int flags,
                                DP_LoadFixedLayerFn on_fixed_layer, void *user,
                                DP_LoadResult *out_result)
{
    DP_ZipReader *zr = DP_zip_reader_new(path);
    if (!zr) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return NULL;
    }

    if (!ora_check_mimetype(zr)) {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_MIMETYPE);
        DP_zip_reader_free(zr);
        return NULL;
    }

    DP_ReadOraContext c = {
        dc,
        {on_fixed_layer, user},
        zr,
        NULL,
        DP_READ_ORA_EXPECT_IMAGE,
        NULL,
        true,
        true,
        true,
        true,
        false,
        START_ID,
        START_ID,
        START_ID,
        0,
        DP_QUEUE_NULL,
        DP_QUEUE_NULL,
        DP_QUEUE_NULL,
        DP_VECTOR_NULL,
        0,
        0,
        NULL,
    };
    DP_queue_init(&c.groups, 8, sizeof(DP_ReadOraGroup));
    DP_queue_init(&c.children, 8, sizeof(DP_ReadOraChildren));
    DP_queue_init(&c.annotations, 8, sizeof(DP_Annotation *));
    DP_CanvasState *cs = ora_read_stack_xml(&c, flags);
    DP_queue_each(&c.children, sizeof(DP_ReadOraChildren), dispose_child, NULL);
    DP_queue_dispose(&c.children);
    DP_queue_dispose(&c.groups);
    DP_queue_each(&c.annotations, sizeof(DP_Annotation *), dispose_annotation,
                  NULL);
    DP_queue_dispose(&c.annotations);
    DP_VECTOR_CLEAR_DISPOSE_TYPE(&c.tracks, DP_ReadOraTrack, dispose_track);
    DP_free(c.text);
    DP_zip_reader_free(zr);
    if (cs) {
        return cs;
    }
    else {
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }
}


DP_CanvasState *load_flat_image(DP_DrawContext *dc, DP_Input *input,
                                const char *flat_image_layer_title,
                                const unsigned char *buf, size_t size,
                                DP_LoadResult *out_result,
                                DP_SaveImageType *out_type)
{
    DP_ImageFileType type;
    DP_Image *img = DP_image_new_from_file_guess(input, buf, size, &type);
    if (!img) {
        assign_load_result(out_result, type == DP_IMAGE_FILE_TYPE_UNKNOWN
                                           ? DP_LOAD_RESULT_UNKNOWN_FORMAT
                                           : DP_LOAD_RESULT_READ_ERROR);
        assign_type(out_type, DP_SAVE_IMAGE_UNKNOWN);
        return NULL;
    }

    switch (type) {
    case DP_IMAGE_FILE_TYPE_PNG:
        assign_type(out_type, DP_SAVE_IMAGE_PNG);
        break;
    case DP_IMAGE_FILE_TYPE_JPEG:
        assign_type(out_type, DP_SAVE_IMAGE_JPEG);
        break;
    default:
        assign_type(out_type, DP_SAVE_IMAGE_UNKNOWN);
        break;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_background_tile_set_noinc(
        tcs, DP_tile_new_from_bgra(0, 0xffffffffu), true);

    int width = DP_image_width(img);
    int height = DP_image_height(img);
    DP_transient_canvas_state_width_set(tcs, width);
    DP_transient_canvas_state_height_set(tcs, height);

    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(width, height, NULL);
    DP_transient_layer_content_put_image(tlc, 1, DP_BLEND_MODE_REPLACE, 0, 0,
                                         img);
    DP_image_free(img);

    DP_TransientLayerList *tll =
        DP_transient_canvas_state_transient_layers(tcs, 1);
    DP_transient_layer_list_insert_transient_content_noinc(tll, tlc, 0);

    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init(START_ID, false);
    const char *title =
        flat_image_layer_title ? flat_image_layer_title : "Layer 1";
    DP_transient_layer_props_title_set(tlp, title, strlen(title));

    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 1);
    DP_transient_layer_props_list_insert_transient_noinc(tlpl, tlp, 0);

    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);

    assign_load_result(out_result, DP_LOAD_RESULT_SUCCESS);
    return DP_transient_canvas_state_persist(tcs);
}


static bool guess_zip(const unsigned char *buf, size_t size)
{
    return size >= 4 && buf[0] == 0x50 && buf[1] == 0x4B && buf[2] == 0x03
        && (buf[3] == 0x04 || buf[3] == 0x06 || buf[3] == 0x08);
}

static bool guess_psd(const unsigned char *buf, size_t size)
{
    return size >= 4 && buf[0] == 0x38 && buf[1] == 0x42 && buf[2] == 0x50
        && buf[3] == 0x53;
}

static DP_CanvasState *load(DP_DrawContext *dc, const char *path,
                            const char *flat_image_layer_title,
                            unsigned int flags, DP_LoadResult *out_result,
                            DP_SaveImageType *out_type)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        assign_type(out_type, DP_SAVE_IMAGE_UNKNOWN);
        return NULL;
    }

    unsigned char buf[8];
    bool error;
    size_t read = DP_input_read(input, buf, sizeof(buf), &error);
    if (error) {
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        assign_type(out_type, DP_SAVE_IMAGE_UNKNOWN);
        return NULL;
    }

    // We could also check if there's an uncompressed mimetype file at the
    // beginning of the ZIP archive like the spec says, but since we only
    // support one ZIP-based format, that's not necessary and would preclude ORA
    // files with the pretty unimportant defect of compressing a file wrong.
    if (guess_zip(buf, read)) {
        DP_input_free(input);
        assign_type(out_type, DP_SAVE_IMAGE_ORA);
        return load_ora(dc, path, flags, NULL, NULL, out_result);
    }

    if (guess_psd(buf, read)) {
        assign_type(out_type, DP_SAVE_IMAGE_PSD);
        if (DP_input_rewind_by(input, read)) {
            return DP_load_psd(dc, input, out_result);
        }
        else {
            assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
            return NULL;
        }
    }

    DP_CanvasState *cs = load_flat_image(dc, input, flat_image_layer_title, buf,
                                         read, out_result, out_type);
    DP_input_free(input);
    return cs;
}

DP_CanvasState *DP_load(DP_DrawContext *dc, const char *path,
                        const char *flat_image_layer_title, unsigned int flags,
                        DP_LoadResult *out_result, DP_SaveImageType *out_type)
{
    if (path) {
        DP_PERF_BEGIN_DETAIL(fn, "image", "path=%s,flags=%x", path, flags);
        DP_CanvasState *cs =
            load(dc, path, flat_image_layer_title, flags, out_result, out_type);
        DP_PERF_END(fn);
        return cs;
    }
    else {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        assign_type(out_type, DP_SAVE_IMAGE_UNKNOWN);
        return NULL;
    }
}


DP_CanvasState *DP_load_ora(DP_DrawContext *dc, const char *path,
                            unsigned int flags,
                            DP_LoadFixedLayerFn on_fixed_layer, void *user,
                            DP_LoadResult *out_result)
{
    return load_ora(dc, path, flags, on_fixed_layer, user, out_result);
}


DP_Player *DP_load_recording(const char *path, DP_LoadResult *out_result)
{
    if (path) {
        DP_PERF_BEGIN_DETAIL(fn, "recording", "path=%s", path);
        DP_Input *input = DP_file_input_new_from_path(path);
        DP_Player *player;
        if (input) {
            player =
                DP_player_new(DP_PLAYER_TYPE_GUESS, path, input, out_result);
            if (player && !DP_player_compatible(player)) {
                DP_error_set("Incompatible recording");
                DP_player_free(player);
                player = NULL;
                assign_load_result(out_result,
                                   DP_LOAD_RESULT_RECORDING_INCOMPATIBLE);
            }
        }
        else {
            player = NULL;
            assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        }
        DP_PERF_END(fn);
        return player;
    }
    else {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        return NULL;
    }
}

DP_Player *DP_load_debug_dump(const char *path, DP_LoadResult *out_result)
{
    if (path) {
        DP_PERF_BEGIN_DETAIL(fn, "dump", "path=%s", path);
        DP_Input *input = DP_file_input_new_from_path(path);
        DP_Player *player;
        if (input) {
            return DP_player_new(DP_PLAYER_TYPE_DEBUG_DUMP, NULL, input,
                                 out_result);
        }
        else {
            player = NULL;
            assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        }
        DP_PERF_END(fn);
        return player;
    }
    else {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        return NULL;
    }
}
