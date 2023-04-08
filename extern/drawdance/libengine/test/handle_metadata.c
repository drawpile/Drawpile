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
#include <dpengine/document_metadata.h>


static void dump_metadata(DP_Output *output, DP_CanvasHistory *ch,
                          const char *title)
{
    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
    DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);

    DP_output_format(output, "\n-- %s\n", title);
    DP_output_format(output, "dpix: %d\n", DP_document_metadata_dpix(dm));
    DP_output_format(output, "dpiy: %d\n", DP_document_metadata_dpiy(dm));
    DP_output_format(output, "framerate: %d\n",
                     DP_document_metadata_framerate(dm));
    DP_output_format(output, "use_timeline: %s\n",
                     DP_document_metadata_use_timeline(dm) ? "true" : "false");
    DP_output_write(output, "\n", 1);

    DP_output_flush(output);
    DP_canvas_state_decref(cs);
}


static void add_handle_metadata_int(DP_Output *output, DP_CanvasHistory *ch,
                                    DP_DrawContext *dc, uint8_t field,
                                    int32_t value)
{
    add_message(output, ch, dc, DP_msg_set_metadata_int_new(1, field, value));
}

static void add_handle_metadata_str(DP_Output *output, DP_CanvasHistory *ch,
                                    DP_DrawContext *dc, uint8_t field,
                                    const char *value)
{
    add_message(output, ch, dc,
                DP_msg_set_metadata_str_new(1, field, value, strlen(value)));
}


static void handle_metadata(DP_Output *output, DP_CanvasHistory *ch,
                            DP_DrawContext *dc)
{
    dump_metadata(output, ch, "initial metadata");

    add_undo_point(output, ch, dc);
    add_handle_metadata_int(output, ch, dc, DP_MSG_SET_METADATA_INT_FIELD_DPIX,
                            1024);
    dump_metadata(output, ch, "set dpix to 1024");

    add_undo_point(output, ch, dc);
    add_handle_metadata_int(output, ch, dc, DP_MSG_SET_METADATA_INT_FIELD_DPIY,
                            99999);
    dump_metadata(output, ch, "set dpiy to 99999");

    add_undo_point(output, ch, dc);
    add_handle_metadata_int(output, ch, dc,
                            DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE, 60);
    dump_metadata(output, ch, "set framerate to 60");

    add_undo_point(output, ch, dc);
    add_handle_metadata_int(output, ch, dc,
                            DP_MSG_SET_METADATA_INT_FIELD_USE_TIMELINE, 1);
    dump_metadata(output, ch, "enable timeline");

    add_undo_point(output, ch, dc);
    add_handle_metadata_int(output, ch, dc, DP_MSG_SET_METADATA_INT_FIELD_DPIX,
                            96);
    add_handle_metadata_int(output, ch, dc, DP_MSG_SET_METADATA_INT_FIELD_DPIY,
                            96);
    add_handle_metadata_int(output, ch, dc,
                            DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE, 120);
    add_handle_metadata_int(output, ch, dc,
                            DP_MSG_SET_METADATA_INT_FIELD_USE_TIMELINE, 0);
    dump_metadata(output, ch, "set all metadata at once");

    add_undo(output, ch, dc);
    dump_metadata(output, ch, "undo metadata settage");

    add_redo(output, ch, dc);
    dump_metadata(output, ch, "redo metadata settage");

    add_undo_point(output, ch, dc);
    add_handle_metadata_int(output, ch, dc, 255, 0);
    dump_metadata(output, ch, "setting invalid int metadata changes nothing");

    add_undo_point(output, ch, dc);
    add_handle_metadata_str(output, ch, dc, 0, "xyzzy");
    dump_metadata(output, ch, "setting string metadata changes nothing");
}


int main(int argc, char **argv)
{
    static DP_HandleTest tests[] = {
        {"handle_metadata", "test/tmp/handle_metadata",
         "test/data/handle_metadata", handle_metadata},
        {NULL, NULL, NULL, NULL},
    };
    return DP_test_main(argc, argv, register_handle_tests, tests);
}
