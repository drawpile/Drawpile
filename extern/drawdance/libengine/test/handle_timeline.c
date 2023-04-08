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
 */
#include "handle_common.h"
#include <dpengine/frame.h>
#include <dpengine/timeline.h>


static void dump_timeline(DP_Output *output, DP_CanvasHistory *ch,
                          const char *title)
{
    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);

    DP_output_format(output, "\n-- %s\n", title);
    int frame_count = DP_timeline_frame_count(tl);
    DP_output_format(output, "%d frame(s)\n", frame_count);

    for (int i = 0; i < frame_count; ++i) {
        DP_Frame *f = DP_timeline_frame_at_noinc(tl, i);
        int layer_id_count = DP_frame_layer_id_count(f);
        DP_output_format(output, "[%d] %d layer(s):", i, layer_id_count);
        for (int j = 0; j < layer_id_count; ++j) {
            DP_output_format(output, " %d", DP_frame_layer_id_at(f, j));
        }
        DP_output_write(output, "\n", 1);
    }

    DP_output_write(output, "\n", 1);
    DP_output_flush(output);
    DP_canvas_state_decref(cs);
}


static void set_layer_ids(int count, uint16_t *out, void *user)
{
    int *layer_ids = user;
    DP_ASSERT(layer_ids[count] == -1);
    for (int i = 0; i < count; ++i) {
        int layer_id = layer_ids[i];
        DP_ASSERT(layer_id >= 0);
        DP_ASSERT(layer_id <= UINT16_MAX);
        out[i] = DP_int_to_uint16(layer_id);
    }
}

static int count_layer_ids(int *layer_ids)
{
    int i = 0;
    while (layer_ids[i] != -1) {
        ++i;
    }
    return i;
}

static void set_timeline_frame(DP_Output *output, DP_CanvasHistory *ch,
                               DP_DrawContext *dc, uint16_t frame, bool insert,
                               int *layer_ids)
{
    add_message(output, ch, dc,
                DP_msg_set_timeline_frame_new(1, frame, insert, set_layer_ids,
                                              count_layer_ids(layer_ids),
                                              layer_ids));
}

static void remove_timeline_frame(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t frame)
{
    add_message(output, ch, dc, DP_msg_remove_timeline_frame_new(1, frame));
}


static void handle_timeline(DP_Output *output, DP_CanvasHistory *ch,
                            DP_DrawContext *dc)
{
    dump_timeline(output, ch, "initial timeline");

    set_timeline_frame(output, ch, dc, 0, true, (int[]){1, 2, 3, -1});
    dump_timeline(output, ch, "append first frame with layers");

    set_timeline_frame(output, ch, dc, 1, true, (int[]){-1});
    dump_timeline(output, ch, "append second frame without layers");

    set_timeline_frame(
        output, ch, dc, 0, true,
        (int[]){1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, -1});
    dump_timeline(output, ch, "insert frame at beginning with lots of layers");

    set_timeline_frame(output, ch, dc, 1, true, (int[]){1, -1});
    dump_timeline(output, ch, "insert frame in middle with single layer");

    set_timeline_frame(output, ch, dc, 3, false, (int[]){9, 99, 999, -1});
    dump_timeline(output, ch, "change last frame layers");

    remove_timeline_frame(output, ch, dc, 3);
    dump_timeline(output, ch, "remove last frame");

    remove_timeline_frame(output, ch, dc, 1);
    dump_timeline(output, ch, "remove second frame");

    remove_timeline_frame(output, ch, dc, 0);
    dump_timeline(output, ch, "remove first frame");

    set_timeline_frame(output, ch, dc, 1, false, (int[]){1, -1});
    dump_timeline(output, ch, "changing frame at end appends it");

    remove_timeline_frame(output, ch, dc, 2);
    dump_timeline(output, ch, "remove nonexistent frame does nothing");

    set_timeline_frame(output, ch, dc, 3, true, (int[]){-1});
    dump_timeline(output, ch, "adding frame beyond end does nothing");

    set_timeline_frame(output, ch, dc, 3, true, (int[]){-1});
    dump_timeline(output, ch, "changing frame beyond end does nothing");
}


int main(int argc, char **argv)
{
    static DP_HandleTest tests[] = {
        {"handle_timeline", "test/tmp/handle_timeline",
         "test/data/handle_timeline", handle_timeline},
        {NULL, NULL, NULL, NULL},
    };
    return DP_test_main(argc, argv, register_handle_tests, tests);
}
