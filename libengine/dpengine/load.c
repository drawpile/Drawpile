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
#include "load.h"
#include "annotation.h"
#include "canvas_state.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "frame.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "ops.h"
#include "player.h"
#include "tile.h"
#include "timeline.h"
#include "xml_stream.h"
#include "zip_archive.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/blend_mode.h>
#include <ctype.h>


#define DRAWPILE_NAMESPACE "http://drawpile.net/"
#define MYPAINT_NAMESPACE  "http://mypaint.org/ns/openraster"


const DP_LoadFormat *DP_load_supported_formats(void)
{
    static const char *ora_ext[] = {"ora", NULL};
    static const char *png_ext[] = {"png", NULL};
    static const char *jpeg_ext[] = {"jpg", "jpeg", NULL};
    static const DP_LoadFormat formats[] = {
        {"OpenRaster", ora_ext},
        {"PNG", png_ext},
        {"JPEG", jpeg_ext},
        {NULL, NULL},
    };
    return formats;
}


static void assign_load_result(DP_LoadResult *out_result, DP_LoadResult result)
{
    if (out_result) {
        *out_result = result;
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
    DP_READ_ORA_EXPECT_FRAME,
    DP_READ_ORA_EXPECT_FRAME_CONTENT,
    DP_READ_ORA_EXPECT_END,
} DP_ReadOraExpect;

typedef enum DP_ReadOraBufferState {
    DP_READ_ORA_BUFFER_STATE_NONE,
    DP_READ_ORA_BUFFER_STATE_LAYERS,
    DP_READ_ORA_BUFFER_STATE_ANNOTATIONS,
} DP_ReadOraBufferState;

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

typedef struct DP_ReadOraContext {
    DP_DrawContext *dc;
    DP_ZipReader *zr;
    DP_Worker *worker;
    DP_ReadOraExpect expect;
    DP_TransientCanvasState *tcs;
    bool want_layers;
    bool want_annotations;
    bool want_timeline;
    int next_id;
    int garbage_depth;
    DP_ReadOraBufferState buffer_state;
    union {
        struct {
            DP_Queue groups;
            DP_Queue children;
        };
        struct {
            DP_Queue annotations;
        };
    };
    int frame_count;
    DP_Queue frames;
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

static void read_ora_context_buffer_dispose(DP_ReadOraContext *c)
{
    switch (c->buffer_state) {
    case DP_READ_ORA_BUFFER_STATE_LAYERS:
        DP_queue_each(&c->children, sizeof(DP_ReadOraChildren), dispose_child,
                      NULL);
        DP_queue_dispose(&c->children);
        DP_queue_dispose(&c->groups);
        break;
    case DP_READ_ORA_BUFFER_STATE_ANNOTATIONS:
        DP_queue_each(&c->annotations, sizeof(DP_Annotation *),
                      dispose_annotation, NULL);
        DP_queue_dispose(&c->annotations);
    default:
        break;
    }
}

static void read_ora_context_init_layers(DP_ReadOraContext *c)
{
    read_ora_context_buffer_dispose(c);
    DP_queue_init(&c->groups, 8, sizeof(DP_ReadOraGroup));
    DP_queue_init(&c->children, 8, sizeof(DP_ReadOraChildren));
    c->buffer_state = DP_READ_ORA_BUFFER_STATE_LAYERS;
}

static void read_ora_context_init_annotations(DP_ReadOraContext *c)
{
    read_ora_context_buffer_dispose(c);
    DP_queue_init(&c->annotations, 8, sizeof(DP_Annotation *));
    c->buffer_state = DP_READ_ORA_BUFFER_STATE_ANNOTATIONS;
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

    read_ora_context_init_layers(c);
    push_group(c, NULL, NULL);
    c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
    return true;
}

static int ora_get_next_id(DP_ReadOraContext *c)
{
    int layer_id = c->next_id++;
    if (layer_id <= UINT16_MAX) {
        return layer_id;
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

    DP_transient_canvas_state_background_tile_set_noinc(
        c->tcs, DP_tile_new_from_pixels8(0, DP_image_pixels(img)));
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
    DP_worker_push(c->worker, &params);
}

static bool ora_handle_layer(DP_ReadOraContext *c, DP_XmlElement *element)
{
    if (ora_try_load_background_tile(c, element)) {
        return true;
    }

    int layer_id = ora_get_next_id(c);
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
    c->expect = DP_READ_ORA_EXPECT_LAYER_END;
    return true;
}

static bool ora_handle_stack(DP_ReadOraContext *c, DP_XmlElement *element)
{
    int layer_id = ora_get_next_id(c);
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
        c->expect = DP_READ_ORA_EXPECT_IMAGE_END;
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

static bool ora_handle_annotation(DP_ReadOraContext *c, DP_XmlElement *element)
{
    int annotation_id = ora_get_next_id(c);
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

    DP_TransientAnnotation **pp = DP_queue_push(&c->annotations, sizeof(ta));
    *pp = ta;

    c->text_len = 0;
    c->expect = DP_READ_ORA_EXPECT_ANNOTATION_CONTENT;
    return true;
}

static void ora_handle_annotation_end(DP_ReadOraContext *c)
{
    DP_TransientAnnotation *ta = *(DP_TransientAnnotation **)DP_queue_peek_last(
        &c->annotations, sizeof(ta));
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
        DP_Annotation *a =
            *(DP_Annotation **)DP_queue_peek(&c->annotations, sizeof(a));
        DP_transient_annotation_list_insert_noinc(tal, a, i);
        DP_queue_shift(&c->annotations);
    }
    c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
}

static void ora_handle_timeline(DP_ReadOraContext *c, DP_XmlElement *element)
{
    const char *enabled = DP_xml_element_attribute(element, NULL, "enabled");
    if (enabled && DP_str_equal(enabled, "true")) {
        DP_TransientDocumentMetadata *tdm =
            DP_transient_canvas_state_transient_metadata(c->tcs);
        DP_transient_document_metadata_use_timeline_set(tdm, true);
    }
    DP_queue_init(&c->frames, 8, sizeof(int));
    c->expect = DP_READ_ORA_EXPECT_FRAME;
}

static void push_frame(DP_ReadOraContext *c, int i)
{
    *(int *)DP_queue_push(&c->frames, sizeof(int)) = i;
}

static bool is_nul_or_space(char c)
{
    return c == '\0' || isspace(c);
}

static void ora_handle_frame_end(DP_ReadOraContext *c)
{
    size_t count_index = c->frames.used;
    push_frame(c, 0);
    ++c->frame_count;

    if (c->text_len != 0) {
        int count = 0;
        char *text = c->text;
        while (*text != '\0') {
            if (isspace(*text)) {
                ++text;
            }
            else {
                char *end;
                long value = strtol(text, &end, 10);
                if (is_nul_or_space(*end)) {
                    if (value >= 0 && value <= UINT16_MAX) {
                        push_frame(c, DP_long_to_int(value));
                        ++count;
                    }
                    else {
                        DP_warn("Read ORA: frame index %ld out of bounds",
                                value);
                    }
                }
                else {
                    DP_warn("Read ORA: error parsing frame number '%.*s'",
                            (int)(end - text), text);
                    while (!is_nul_or_space(*end)) {
                        ++end;
                    }
                }
                text = end;
            }
        }
        *(int *)DP_queue_at(&c->frames, sizeof(int), count_index) = count;
    }

    c->expect = DP_READ_ORA_EXPECT_FRAME;
}

static int frame_at(DP_ReadOraContext *c, size_t i)
{
    return *(int *)DP_queue_at(&c->frames, sizeof(int), i);
}

static void ora_fill_timeline(DP_ReadOraContext *c)
{
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(c->tcs, c->frame_count);
    size_t i = 0;
    size_t count = c->frames.used;
    int frame_index = 0;
    while (i < count) {
        int layer_id_count = frame_at(c, i++);
        DP_TransientFrame *tf = DP_transient_frame_new_init(layer_id_count);
        for (int j = 0; j < layer_id_count; ++j) {
            int layer_id = frame_at(c, i++) + 1;
            DP_transient_frame_layer_id_set_at(tf, layer_id, j);
        }
        DP_transient_timeline_insert_transient_noinc(ttl, tf, frame_index++);
    }
    // The timeline might have had invalid or duplicate entries, clean it up.
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
                c->next_id = 1;
            }
            else {
                DP_warn("Duplicate root <stack>");
                ++c->garbage_depth;
            }
        }
        else if (DP_xml_element_name_equals(element, DRAWPILE_NAMESPACE,
                                            "annotations")) {
            if (c->want_annotations) {
                c->want_annotations = false;
                c->expect = DP_READ_ORA_EXPECT_ANNOTATION;
                c->next_id = 1;
                read_ora_context_init_annotations(c);
            }
            else {
                DP_warn("Duplicate <drawpile:annotations>");
                ++c->garbage_depth;
            }
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
        else {
            DP_debug("Expected <stack> or <layer>, got <%s:%s>",
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
    case DP_READ_ORA_EXPECT_FRAME:
        if (DP_xml_element_name_equals(element, NULL, "frame")) {
            c->text_len = 0;
            c->expect = DP_READ_ORA_EXPECT_FRAME_CONTENT;
        }
        else {
            DP_debug("Expected <frame> got <%s:%s>",
                     DP_xml_element_namespace(element),
                     DP_xml_element_name(element));
            ++c->garbage_depth;
        }
        break;
    case DP_READ_ORA_EXPECT_FRAME_CONTENT:
        DP_debug("Expected text or </frame>, got <%s:%s>",
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
    case DP_READ_ORA_EXPECT_ANNOTATION_CONTENT:
    case DP_READ_ORA_EXPECT_FRAME_CONTENT: {
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
    case DP_READ_ORA_EXPECT_FRAME:
        c->expect = DP_READ_ORA_EXPECT_ROOT_STACK_OR_ANNOTATIONS_OR_TIMELINE;
        break;
    case DP_READ_ORA_EXPECT_FRAME_CONTENT:
        ora_handle_frame_end(c);
        break;
    case DP_READ_ORA_EXPECT_IMAGE:
    case DP_READ_ORA_EXPECT_END:
        break;
    }
    return true;
}

static DP_CanvasState *ora_read_stack_xml(DP_ReadOraContext *c)
{
    DP_ZipReaderFile *zrf = DP_zip_reader_read_file(c->zr, "stack.xml");
    if (!zrf) {
        return NULL;
    }

    c->worker =
        DP_worker_new(64, sizeof(struct DP_OraLoadLayerContentParams),
                      DP_thread_cpu_count(), ora_load_layer_content_job);
    if (!c->worker) {
        DP_zip_reader_file_free(zrf);
        return NULL;
    }

    c->tcs = DP_transient_canvas_state_new_init();
    bool xml_ok = DP_xml_stream(
        DP_zip_reader_file_size(zrf), DP_zip_reader_file_content(zrf),
        ora_xml_start_element, ora_xml_text_content, ora_xml_end_element, c);
    DP_zip_reader_file_free(zrf);

    if (xml_ok && !c->want_layers) {
        DP_transient_canvas_state_layer_routes_reindex(c->tcs, c->dc);
        DP_worker_free_join(c->worker);
        if (!c->want_timeline) {
            ora_fill_timeline(c);
        }
        return DP_transient_canvas_state_persist(c->tcs);
    }
    else {
        DP_worker_free_join(c->worker);
        DP_transient_canvas_state_decref_nullable(c->tcs);
        read_ora_context_buffer_dispose(c);
        return NULL;
    }
}

static DP_CanvasState *load_ora(DP_DrawContext *dc, const char *path,
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
        zr,
        NULL,
        DP_READ_ORA_EXPECT_IMAGE,
        NULL,
        true,
        true,
        true,
        0,
        0,
        DP_READ_ORA_BUFFER_STATE_NONE,
        {{DP_QUEUE_NULL, DP_QUEUE_NULL}},
        0,
        DP_QUEUE_NULL,
        0,
        0,
        NULL,
    };
    DP_CanvasState *cs = ora_read_stack_xml(&c);
    read_ora_context_buffer_dispose(&c);
    DP_queue_dispose(&c.frames);
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
                                DP_LoadResult *out_result)
{
    DP_ImageFileType type;
    DP_Image *img =
        DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS, &type);
    if (!img) {
        assign_load_result(out_result, type == DP_IMAGE_FILE_TYPE_UNKNOWN
                                           ? DP_LOAD_RESULT_UNKNOWN_FORMAT
                                           : DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_background_tile_set_noinc(
        tcs, DP_tile_new_from_bgra(0, 0xffffffffu));

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
        DP_transient_layer_props_new_init(0x100, false);
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


DP_CanvasState *DP_load(DP_DrawContext *dc, const char *path,
                        const char *flat_image_layer_title,
                        DP_LoadResult *out_result)
{
    if (!path) {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        return NULL;
    }

    const char *dot = strrchr(path, '.');
    if (DP_str_equal_lowercase(dot, ".ora")) {
        return load_ora(dc, path, out_result);
    }

    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return NULL;
    }

    DP_CanvasState *cs =
        load_flat_image(dc, input, flat_image_layer_title, out_result);

    DP_input_free(input);
    return cs;
}


DP_Player *DP_load_recording(const char *path, DP_LoadResult *out_result)
{
    if (!path) {
        assign_load_result(out_result, DP_LOAD_RESULT_BAD_ARGUMENTS);
        return NULL;
    }
    return DP_player_new(path, out_result);
}
