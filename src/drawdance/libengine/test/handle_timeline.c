// SPDX-License-Identifier: MIT
#include "handle_common.h"
#include <dpengine/document_metadata.h>
#include <dpengine/key_frame.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpengine/timeline.h>
#include <dpengine/track.h>


static void dump_layer_list(DP_Output *output, DP_LayerPropsList *lpl,
                            int indent);

static void dump_layer(DP_Output *output, DP_LayerProps *lp, int indent)
{
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    format_indent(output, indent, "%s %d %s\n", child_lpl ? "group" : "layer",
                  DP_layer_props_id(lp), DP_layer_props_title(lp, NULL));
    if (child_lpl) {
        dump_layer_list(output, child_lpl, indent + 1);
    }
}

static void dump_layer_list(DP_Output *output, DP_LayerPropsList *lpl,
                            int indent)
{
    int count = DP_layer_props_list_count(lpl);
    for (int i = 0; i < count; ++i) {
        dump_layer(output, DP_layer_props_list_at_noinc(lpl, i), indent);
    }
}

static void dump_timeline(DP_Output *output, DP_CanvasHistory *ch,
                          const char *title, bool dump_layers)
{
    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);

    DP_output_format(output, "\n-- %s\n", title);

    DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);
    DP_output_format(output, "frame_count: %d\n",
                     DP_document_metadata_frame_count(dm));

    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);
    DP_output_format(output, "%d track(s)\n", track_count);

    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        int kf_count = DP_track_key_frame_count(t);
        format_indent(output, 1, "[%d] %d \"%s\" %d key frame(s):\n", i,
                      DP_track_id(t), DP_track_title(t, NULL), kf_count);
        for (int j = 0; j < kf_count; ++j) {
            DP_KeyFrame *kf = DP_track_key_frame_at_noinc(t, j);
            int frame_index = DP_track_frame_index_at_noinc(t, j);
            format_indent(output, 2, "[%d] key on layer %d at %d", j,
                          DP_key_frame_layer_id(kf), frame_index);
            size_t kf_title_length;
            const char *kf_title = DP_key_frame_title(kf, &kf_title_length);
            if (kf_title_length != 0) {
                DP_output_format(output, " \"%.*s\"",
                                 DP_size_to_int(kf_title_length), kf_title);
            }
            int layer_count;
            const DP_KeyFrameLayer *kfls =
                DP_key_frame_layers(kf, &layer_count);
            if (layer_count == 0) {
                DP_OUTPUT_PRINT_LITERAL(output, "\n");
            }
            else {
                DP_output_format(output, " %d layer flag(s):\n", layer_count);
                for (int k = 0; k < layer_count; ++k) {
                    DP_KeyFrameLayer kfl = kfls[k];
                    format_indent(output, 3, "[%d] layer %d flags 0x%x\n", k,
                                  kfl.layer_id, kfl.flags);
                }
            }
        }
    }

    if (dump_layers) {
        DP_OUTPUT_PRINT_LITERAL(output, "layers:\n");
        dump_layer_list(output, DP_canvas_state_layer_props_noinc(cs), 1);
    }

    DP_output_write(output, "\n", 1);
    DP_output_flush(output);
    DP_canvas_state_decref(cs);
}


static void add_layer_tree_create(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t id,
                                  const char *name, bool group,
                                  uint16_t parent_id)
{
    uint8_t flags =
        DP_int_to_uint8(group ? DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP : 0)
        | DP_int_to_uint8(parent_id == 0 ? 0
                                         : DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO);
    add_message(output, ch, dc,
                DP_msg_layer_tree_create_new(1, id, 0, parent_id, 0, flags,
                                             name, strlen(name)));
}

static void add_layer_tree_delete(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t id)
{
    add_message(output, ch, dc, DP_msg_layer_tree_delete_new(1, id, 0));
}

static void add_set_metadata_frame_count(DP_Output *output,
                                         DP_CanvasHistory *ch,
                                         DP_DrawContext *dc,
                                         int32_t frame_count)
{
    add_message(output, ch, dc,
                DP_msg_set_metadata_int_new(
                    1, DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT, frame_count));
}

static void add_track_create(DP_Output *output, DP_CanvasHistory *ch,
                             DP_DrawContext *dc, uint16_t id,
                             uint16_t insert_id, uint16_t source_id,
                             const char *title)
{
    add_message(output, ch, dc,
                DP_msg_track_create_new(1, id, insert_id, source_id,
                                        title ? title : "",
                                        title ? strlen(title) : 0));
}

static void set_uint16s_from_int(int count, uint16_t *out, void *user)
{
    int *tracks = user;
    for (int i = 0; i < count; ++i) {
        out[i] = DP_int_to_uint16(tracks[i]);
    }
}

static void add_track_order(DP_Output *output, DP_CanvasHistory *ch,
                            DP_DrawContext *dc, int *tracks)
{
    int count = 0;
    while (tracks[count] > 0) {
        ++count;
    }
    add_message(output, ch, dc,
                DP_msg_track_order_new(1, set_uint16s_from_int, count, tracks));
}

static void add_track_retitle(DP_Output *output, DP_CanvasHistory *ch,
                              DP_DrawContext *dc, uint16_t id,
                              const char *title)
{
    add_message(output, ch, dc,
                DP_msg_track_retitle_new(1, id, title ? title : "",
                                         title ? strlen(title) : 0));
}

static void add_track_delete(DP_Output *output, DP_CanvasHistory *ch,
                             DP_DrawContext *dc, uint16_t id)
{
    add_message(output, ch, dc, DP_msg_track_delete_new(1, id));
}

static void add_key_frame_set(DP_Output *output, DP_CanvasHistory *ch,
                              DP_DrawContext *dc, uint16_t track_id,
                              uint16_t frame_index, uint16_t source_id,
                              uint16_t source_index, uint8_t source)
{
    add_message(output, ch, dc,
                DP_msg_key_frame_set_new(1, track_id, frame_index, source_id,
                                         source_index, source));
}

static void add_key_frame_retitle(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t track_id,
                                  uint16_t frame_index, const char *title)
{
    add_message(output, ch, dc,
                DP_msg_key_frame_retitle_new(1, track_id, frame_index,
                                             title ? title : "",
                                             title ? strlen(title) : 0));
}

static void add_key_frame_layer_attributes(DP_Output *output,
                                           DP_CanvasHistory *ch,
                                           DP_DrawContext *dc,
                                           uint16_t track_id,
                                           uint16_t frame_index, int *layers)
{
    int count = 0;
    while (layers[count] > 0) {
        count += 2;
    }
    add_message(output, ch, dc,
                DP_msg_key_frame_layer_attributes_new(1, track_id, frame_index,
                                                      set_uint16s_from_int,
                                                      count, layers));
}

static void add_key_frame_delete(DP_Output *output, DP_CanvasHistory *ch,
                                 DP_DrawContext *dc, uint16_t track_id,
                                 uint16_t frame_index, uint16_t move_track_id,
                                 uint16_t move_frame_index)
{
    add_message(output, ch, dc,
                DP_msg_key_frame_delete_new(1, track_id, frame_index,
                                            move_track_id, move_frame_index));
}


static void handle_timeline(DP_Output *output, DP_CanvasHistory *ch,
                            DP_DrawContext *dc)
{
    dump_timeline(output, ch, "initial timeline", false);

    add_undo_point(output, ch, dc);
    add_layer_tree_create(output, ch, dc, 257, "L1", false, 0);
    add_layer_tree_create(output, ch, dc, 258, "G2", true, 0);
    add_layer_tree_create(output, ch, dc, 259, "G2/L1", false, 258);
    add_layer_tree_create(output, ch, dc, 260, "G2/L2", false, 258);
    add_layer_tree_create(output, ch, dc, 261, "G2/G3", true, 258);
    add_layer_tree_create(output, ch, dc, 262, "G2/G3/L1", false, 261);
    add_layer_tree_create(output, ch, dc, 263, "G2/G3/L2", false, 261);
    add_layer_tree_create(output, ch, dc, 264, "L3", false, 0);
    add_layer_tree_create(output, ch, dc, 265, "L4", false, 0);
    add_undo_point(output, ch, dc);
    add_set_metadata_frame_count(output, ch, dc, 30);
    dump_timeline(output, ch, "layer setup", true);

    add_undo_point(output, ch, dc);
    add_track_create(output, ch, dc, 300, 0, 0, "Track 1");
    dump_timeline(output, ch, "create track 1", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 0, 257, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "create track 1 key 0", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 20, 0, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "create track 1 key 20 without layer", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 10, 258, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "create track 1 key 10", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 30, 265, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "fail to create track 1 key 30", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 29, 265, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "create track 1 key 29", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 20, 265, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "change track 1 key 20", false);

    add_undo_point(output, ch, dc);
    add_key_frame_retitle(output, ch, dc, 300, 20, "T1 K20");
    dump_timeline(output, ch, "name track 1 key 20", false);

    add_undo_point(output, ch, dc);
    add_key_frame_retitle(output, ch, dc, 300, 20, "Key 20");
    dump_timeline(output, ch, "rename track 1 key 20", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 300, 20, 258, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "change named track layer id", false);

    add_undo_point(output, ch, dc);
    add_key_frame_layer_attributes(output, ch, dc, 300, 20,
                                   (int[]){258, DP_KEY_FRAME_LAYER_HIDDEN, 261,
                                           DP_KEY_FRAME_LAYER_REVEALED, 263,
                                           DP_KEY_FRAME_LAYER_HIDDEN, -1});
    dump_timeline(output, ch, "add track 1 key 20 layer attributes", false);

    add_undo_point(output, ch, dc);
    add_key_frame_layer_attributes(
        output, ch, dc, 300, 20,
        (int[]){258, DP_KEY_FRAME_LAYER_HIDDEN, 263,
                DP_KEY_FRAME_LAYER_REVEALED, 263, DP_KEY_FRAME_LAYER_HIDDEN, 99,
                DP_KEY_FRAME_LAYER_HIDDEN, 257, DP_KEY_FRAME_LAYER_HIDDEN, -1});
    dump_timeline(output, ch,
                  "clobber track 1 key 20 layer attributes, invalid and dupes "
                  "are ignored, layers outside of group are accepted",
                  false);

    add_undo_point(output, ch, dc);
    add_track_create(output, ch, dc, 301, 0, 300, "Track 2");
    dump_timeline(output, ch, "duplicate track 1", false);

    add_undo_point(output, ch, dc);
    add_track_create(output, ch, dc, 302, 300, 0, "Track 0");
    dump_timeline(output, ch, "insert track", false);

    add_undo_point(output, ch, dc);
    add_key_frame_retitle(output, ch, dc, 300, 20, NULL);
    dump_timeline(output, ch, "unname track 1 key 20", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 300, 20, 0, 0);
    dump_timeline(output, ch, "delete track 1 key 20", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 300, 20, 0, 0);
    dump_timeline(output, ch, "attempt to delete track 1 key 20 again", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 300, 29, 0, 0);
    dump_timeline(output, ch, "delete track 1 key 29", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 300, 0, 0, 0);
    dump_timeline(output, ch, "delete track 1 key 0", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 300, 10, 0, 0);
    dump_timeline(output, ch, "delete track 1 key 10", false);

    add_undo_point(output, ch, dc);
    add_track_create(output, ch, dc, 303, 300, 301, "Track 4");
    dump_timeline(output, ch, "duplicate and insert track 2", false);

    add_undo_point(output, ch, dc);
    add_track_retitle(output, ch, dc, 303, "Track Four");
    dump_timeline(output, ch, "rename track 4", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 303, 0, 262, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    add_key_frame_set(output, ch, dc, 303, 20, 263, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    add_key_frame_set(output, ch, dc, 303, 29, 261, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
    dump_timeline(output, ch, "change track 4 layers", true);

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 263);
    dump_timeline(output, ch, "delete layer 263", true);

    add_undo_point(output, ch, dc);
    add_layer_tree_delete(output, ch, dc, 258);
    dump_timeline(output, ch, "delete layer 258", true);

    add_undo_point(output, ch, dc);
    add_track_delete(output, ch, dc, 303);
    dump_timeline(output, ch, "delete track 4", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 301, 25, 301, 20,
                      DP_MSG_KEY_FRAME_SET_SOURCE_KEY_FRAME);
    dump_timeline(output, ch, "copy track 2 key 20 to 25", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 301, 25, 300, 3);
    dump_timeline(output, ch, "move track 2 key 25 to track 1 key 3", false);

    add_undo_point(output, ch, dc);
    add_key_frame_delete(output, ch, dc, 300, 3, 300, 0);
    dump_timeline(output, ch, "move track 1 key 3 to track 1 key 0", false);

    add_undo_point(output, ch, dc);
    add_key_frame_set(output, ch, dc, 301, 0, 300, 0,
                      DP_MSG_KEY_FRAME_SET_SOURCE_KEY_FRAME);
    dump_timeline(output, ch, "copy track 1 key 0 to track 2 key 0", false);

    add_undo_point(output, ch, dc);
    add_set_metadata_frame_count(output, ch, dc, 20);
    dump_timeline(output, ch, "decrease frame count truncates", false);

    add_undo_point(output, ch, dc);
    add_set_metadata_frame_count(output, ch, dc, 60);
    dump_timeline(output, ch, "increasing frame count again changes nothing",
                  false);

    add_undo_point(output, ch, dc);
    add_set_metadata_frame_count(output, ch, dc, 0);
    dump_timeline(output, ch, "setting frame count to 0 gives 1", false);

    add_undo_point(output, ch, dc);
    add_set_metadata_frame_count(output, ch, dc, -1);
    dump_timeline(output, ch, "setting frame count to -1 gives 1", false);

    add_undo_point(output, ch, dc);
    add_track_delete(output, ch, dc, 404);
    dump_timeline(output, ch, "delete nonexistent track", false);

    add_undo_point(output, ch, dc);
    add_track_delete(output, ch, dc, 300);
    dump_timeline(output, ch, "delete track 1", false);

    add_undo_point(output, ch, dc);
    add_track_delete(output, ch, dc, 301);
    dump_timeline(output, ch, "delete track 2", false);

    add_undo_point(output, ch, dc);
    add_track_delete(output, ch, dc, 302);
    dump_timeline(output, ch, "delete track 0", true);
}


static void handle_timeline_track_order(DP_Output *output, DP_CanvasHistory *ch,
                                        DP_DrawContext *dc)
{
    dump_timeline(output, ch, "initial timeline", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc, (int[]){-1});
    dump_timeline(output, ch, "ordering empty tracks does nothing", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc, (int[]){100, 200, 300, -1});
    dump_timeline(output, ch,
                  "ordering empty tracks with invalid ids does nothing", false);

    add_undo_point(output, ch, dc);
    add_track_create(output, ch, dc, 100, 0, 0, "Track 1");
    add_track_create(output, ch, dc, 200, 0, 0, "Track 2");
    add_track_create(output, ch, dc, 300, 0, 0, "Track 3");
    add_track_create(output, ch, dc, 400, 0, 0, "Track 4");
    add_track_create(output, ch, dc, 500, 0, 0, "Track 5");
    dump_timeline(output, ch, "create tracks", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc, (int[]){100, 200, 300, 400, 500, -1});
    dump_timeline(output, ch, "order tracks the other way round", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc, (int[]){100, 500, 400, 300, 200, -1});
    dump_timeline(output, ch, "order tracks interleaved", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc, (int[]){-1});
    dump_timeline(output, ch,
                  "ordering tracks with no arguments changes nothing", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc,
                    (int[]){100, 404, 200, 300, 200, 400, 500, 999, 0, -1});
    dump_timeline(output, ch, "duplicates and missing elements ignored", false);

    add_undo_point(output, ch, dc);
    add_track_order(output, ch, dc, (int[]){500, 300, 999, 0, -1});
    dump_timeline(output, ch,
                  "missing elements are appended in the order they appear",
                  false);
}


int main(int argc, char **argv)
{
    static DP_HandleTest tests[] = {
        {"handle_timeline", "test/tmp/handle_timeline",
         "test/data/handle_timeline", handle_timeline},
        {"handle_timeline_track_order", "test/tmp/handle_timeline_track_order",
         "test/data/handle_timeline_track_order", handle_timeline_track_order},
        {NULL, NULL, NULL, NULL},
    };
    return DP_test_main(argc, argv, register_handle_tests, tests);
}
