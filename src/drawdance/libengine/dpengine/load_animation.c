// SPDX-License-Identifier: GPL-3.0-or-later
#include "load_animation.h"
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
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpcommon/vector.h>
#include <dpmsg/blend_mode.h>


#define BASE_LAYER_ID 0x100

static void assign_load_result(DP_LoadResult *out_result, DP_LoadResult result)
{
    if (out_result) {
        *out_result = result;
    }
}

DP_CanvasState *DP_load_animation_frames(
    DP_DrawContext *dc, int path_count, DP_LoadAnimationFramesPathAtFn path_at,
    void *user, uint32_t background_color, int hold_time, int framerate,
    DP_LoadAnimationSetLayerTitleFn set_layer_title,
    DP_LoadAnimationSetGroupTitleFn set_group_title,
    DP_LoadAnimationSetTrackTitleFn set_track_title, DP_LoadResult *out_result)
{
    DP_ASSERT(dc);
    DP_ASSERT(path_at);
    DP_ASSERT(set_layer_title);
    DP_ASSERT(set_group_title);
    DP_ASSERT(set_track_title);

    int layer_count = 0;
    DP_TransientLayerList *child_tll;
    DP_TransientLayerPropsList *child_tlpl;
    DP_TransientTrack *tt;
    int width, height;
    for (int i = 0; i < path_count; ++i) {
        const char *path = path_at(user, i);
        if (!path) {
            DP_warn("Frame path %d is null", i);
            continue;
        }

        DP_Input *input = DP_file_input_new_from_path(path);
        if (!input) {
            DP_warn("Can't open frame path '%s': %s", path, DP_error());
            continue;
        }

        DP_Image *img =
            DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS, NULL);
        DP_input_free(input);
        if (!img) {
            DP_warn("Can't read frame path '%s': %s", path, DP_error());
            continue;
        }

        if (layer_count == 0) {
            width = DP_image_width(img);
            height = DP_image_height(img);
            child_tll = DP_transient_layer_list_new_init(path_count);
            child_tlpl = DP_transient_layer_props_list_new_init(path_count);
            tt = DP_transient_track_new_init(path_count);
            set_track_title(tt, 0);
        }

        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_new_init(width, height, NULL);
        DP_transient_layer_content_put_image(tlc, 0, DP_BLEND_MODE_REPLACE, 0,
                                             0, img);
        DP_image_free(img);
        DP_transient_layer_list_insert_transient_content_noinc(child_tll, tlc,
                                                               layer_count);

        int layer_id = BASE_LAYER_ID + 1 + layer_count;
        DP_TransientLayerProps *tlp =
            DP_transient_layer_props_new_init(layer_id, false);
        set_layer_title(tlp, i);
        DP_transient_layer_props_list_insert_transient_noinc(child_tlpl, tlp,
                                                             layer_count);

        DP_transient_track_set_transient_noinc(
            tt, i * hold_time, DP_transient_key_frame_new_init(layer_id, 0),
            layer_count);

        ++layer_count;
    }

    if (layer_count == 0) {
        DP_error_set("Failed to read any frame images");
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_width_set(tcs, width);
    DP_transient_canvas_state_height_set(tcs, height);

    uint32_t background_alpha = background_color & 0xff000000u;
    if (background_alpha != 0) {
        DP_transient_canvas_state_background_tile_set_noinc(
            tcs, DP_tile_new_from_bgra(0, background_color),
            background_alpha == 0xff000000u);
    }

    DP_TransientLayerList *root_tll =
        DP_transient_canvas_state_transient_layers(tcs, 1);
    DP_transient_layer_list_insert_transient_group_noinc(
        root_tll,
        DP_transient_layer_group_new_init_with_transient_children_noinc(
            width, height, child_tll),
        0);

    DP_TransientLayerProps *group_tlp =
        DP_transient_layer_props_new_init_with_transient_children_noinc(
            BASE_LAYER_ID, child_tlpl);
    set_group_title(group_tlp, 0);

    DP_TransientLayerPropsList *root_tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 1);
    DP_transient_layer_props_list_insert_transient_noinc(root_tlpl, group_tlp,
                                                         0);

    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 1);
    DP_transient_timeline_insert_transient_noinc(ttl, tt, 0);

    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(tcs);
    DP_transient_document_metadata_framerate_set(tdm, framerate);
    DP_transient_document_metadata_frame_count_set(tdm, path_count * hold_time);

    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    DP_transient_canvas_state_timeline_cleanup(tcs);
    return DP_transient_canvas_state_persist(tcs);
}


static void on_fixed_layer(void *user, int layer_id)
{
    DP_Vector *fixed_layer_ids = user;
    DP_VECTOR_PUSH_TYPE(fixed_layer_ids, int, layer_id);
}

static DP_CanvasState *load_ora(DP_DrawContext *dc, const char *path,
                                unsigned int flags, DP_Vector *fixed_layer_ids,
                                DP_LoadResult *out_result)
{
    return DP_load_ora(dc, path, flags, on_fixed_layer, fixed_layer_ids,
                       out_result);
}

static int compare_layer_ids(const void *a, const void *b)
{
    int ai = *(int *)a;
    int bi = *(int *)b;
    return ai < bi ? -1 : ai > bi ? 1 : 0;
}

static bool is_fixed(DP_Vector *fixed_layer_ids, int layer_id)
{
    return fixed_layer_ids->used != 0
        && bsearch(&layer_id, fixed_layer_ids->elements, fixed_layer_ids->used,
                   sizeof(int), compare_layer_ids);
}

static int count_elements(DP_Vector *fixed_layer_ids, DP_LayerPropsList *lpl,
                          int *out_track_count, int *out_key_frame_count)
{
    int track_count = 0;
    int key_frame_count = 0;
    bool in_frame_run = false;
    int layer_count = DP_layer_props_list_count(lpl);

    for (int i = 0; i < layer_count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (is_fixed(fixed_layer_ids, DP_layer_props_id(lp))) {
            ++track_count;
            in_frame_run = false;
        }
        else {
            ++key_frame_count;
            if (!in_frame_run) {
                ++track_count;
                in_frame_run = true;
            }
        }
    }

    *out_track_count = track_count;
    *out_key_frame_count = key_frame_count;
    return layer_count;
}

int count_frame_run(DP_Vector *fixed_layer_ids, DP_LayerPropsList *lpl,
                    int layer_count, int start_index)
{
    int end_index = start_index;
    while (end_index < layer_count) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, end_index);
        if (is_fixed(fixed_layer_ids, DP_layer_props_id(lp))) {
            break;
        }
        ++end_index;
    }
    return end_index - start_index;
}

static DP_CanvasState *convert_animation(
    DP_CanvasState *cs, DP_DrawContext *dc, int hold_time, int framerate,
    DP_LoadAnimationSetGroupTitleFn set_group_title,
    DP_LoadAnimationSetTrackTitleFn set_track_title, DP_Vector *fixed_layer_ids)
{
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int track_count, key_frame_count;
    int layer_count =
        count_elements(fixed_layer_ids, lpl, &track_count, &key_frame_count);

    DP_TransientLayerList *tll = DP_transient_layer_list_new_init(track_count);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(track_count);
    DP_TransientTimeline *ttl = DP_transient_timeline_new_init(track_count);

    int layer_index = 0;
    int group_index = 0;
    int track_index = 0;
    int key_frame_index = 0;
    int next_layer_id = BASE_LAYER_ID;
    int next_track_id = BASE_LAYER_ID;
    while (layer_index < layer_count) {
        // Count frames until the end or the next fixed layer.
        int frame_run =
            count_frame_run(fixed_layer_ids, lpl, layer_count, layer_index);
        // No frames in the run means this must be a fixed layer.
        if (frame_run == 0) {
            DP_transient_layer_list_set_inc(
                tll, DP_layer_list_at_noinc(ll, layer_index), track_index);

            DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, layer_index);
            DP_TransientLayerProps *tlp = DP_transient_layer_props_new(lp);
            DP_transient_layer_props_id_set(tlp, next_layer_id);
            DP_transient_layer_props_list_set_transient_noinc(tlpl, tlp,
                                                              track_index);

            DP_TransientTrack *tt = DP_transient_track_new_init(1);
            DP_transient_track_id_set(tt, next_track_id);
            size_t title_length;
            const char *title = DP_layer_props_title(lp, &title_length);
            DP_transient_track_title_set(tt, title, title_length);
            DP_transient_track_set_transient_noinc(
                tt, 0, DP_transient_key_frame_new_init(next_layer_id, 0), 0);
            DP_transient_timeline_set_transient_noinc(ttl, tt, track_index);

            ++layer_index;
        }
        else {
            DP_TransientLayerList *child_tll =
                DP_transient_layer_list_new_init(frame_run);
            DP_TransientLayerPropsList *child_tlpl =
                DP_transient_layer_props_list_new_init(frame_run);

            bool needs_blank_frame =
                key_frame_index + frame_run < key_frame_count;
            DP_TransientTrack *tt = DP_transient_track_new_init(
                frame_run + (needs_blank_frame ? 1 : 0));
            DP_transient_track_id_set(tt, next_track_id);
            set_track_title(tt, group_index);

            for (int i = 0; i < frame_run; ++i) {
                DP_transient_layer_list_set_inc(
                    child_tll, DP_layer_list_at_noinc(ll, layer_index), i);

                DP_TransientLayerProps *child_tlp =
                    DP_transient_layer_props_new(
                        DP_layer_props_list_at_noinc(lpl, layer_index));
                DP_transient_layer_props_id_set(child_tlp, next_layer_id);
                DP_transient_layer_props_list_set_transient_noinc(child_tlpl,
                                                                  child_tlp, i);

                DP_transient_track_set_transient_noinc(
                    tt, key_frame_index * hold_time,
                    DP_transient_key_frame_new_init(next_layer_id, 0), i);

                ++layer_index;
                ++key_frame_index;
                ++next_layer_id;
            }

            DP_TransientLayerGroup *tlg =
                DP_transient_layer_group_new_init_with_transient_children_noinc(
                    width, height, child_tll);
            DP_transient_layer_list_set_transient_group_noinc(tll, tlg,
                                                              track_index);

            DP_TransientLayerProps *tlp =
                DP_transient_layer_props_new_init_with_transient_children_noinc(
                    next_layer_id, child_tlpl);
            DP_transient_layer_props_isolated_set(tlp, false);
            set_group_title(tlp, group_index);
            DP_transient_layer_props_list_set_transient_noinc(tlpl, tlp,
                                                              track_index);

            if (needs_blank_frame) {
                DP_transient_track_set_transient_noinc(
                    tt, key_frame_index * hold_time,
                    DP_transient_key_frame_new_init(0, 0), frame_run);
            }
            DP_transient_timeline_set_transient_noinc(ttl, tt, track_index);

            ++group_index;
        }

        ++track_index;
        ++next_track_id;
        ++next_layer_id;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(tcs);
    DP_transient_document_metadata_framerate_set(tdm, framerate);
    DP_transient_document_metadata_frame_count_set(
        tdm, DP_max_int(1, key_frame_count * hold_time));
    DP_transient_canvas_state_transient_layers_set_noinc(tcs, tll);
    DP_transient_canvas_state_transient_layer_props_set_noinc(tcs, tlpl);
    DP_transient_canvas_state_transient_timeline_set_noinc(tcs, ttl);
    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_load_animation_layers(
    DP_DrawContext *dc, const char *path, int hold_time, int framerate,
    unsigned int flags, DP_LoadAnimationSetGroupTitleFn set_group_title,
    DP_LoadAnimationSetTrackTitleFn set_track_title, DP_LoadResult *out_result)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return NULL;
    }

    unsigned char buf[8];
    bool error;
    size_t read = DP_input_read(input, buf, sizeof(buf), &error);
    if (error) {
        DP_input_free(input);
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }

    DP_SaveImageType type = DP_load_guess(buf, read);
    DP_CanvasState *cs;
    DP_Vector fixed_layer_ids;
    switch (type) {
    case DP_SAVE_IMAGE_ORA:
        DP_input_free(input);
        DP_VECTOR_INIT_TYPE(&fixed_layer_ids, int, 32);
        cs = load_ora(dc, path, flags, &fixed_layer_ids, out_result);
        break;
    case DP_SAVE_IMAGE_PSD:
        if (DP_input_rewind(input)) {
            cs = DP_load_psd(dc, input, out_result);
            fixed_layer_ids = DP_VECTOR_NULL;
            break;
        }
        else {
            DP_input_free(input);
            assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
            return NULL;
        }
    default:
        DP_error_set("Invalid animation import format type %d", (int)type);
        assign_load_result(out_result, DP_LOAD_RESULT_UNKNOWN_FORMAT);
        return NULL;
    }

    DP_CanvasState *converted_cs;
    if (cs) {
        DP_VECTOR_SORT_TYPE(&fixed_layer_ids, int, compare_layer_ids);
        converted_cs =
            convert_animation(cs, dc, hold_time, framerate, set_group_title,
                              set_track_title, &fixed_layer_ids);
        DP_canvas_state_decref(cs);
    }
    else {
        converted_cs = NULL;
    }

    DP_vector_dispose(&fixed_layer_ids);
    return converted_cs;
}
