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
#include "dpcommon_test.h"
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <dpengine/annotation.h>
#include <dpengine/annotation_list.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpmsg/message.h>
#include <dpmsg/messages/annotation_create.h>
#include <dpmsg/messages/annotation_delete.h>
#include <dpmsg/messages/annotation_edit.h>
#include <dpmsg/messages/annotation_reshape.h>
#include <dpmsg/messages/undo.h>
#include <dpmsg/messages/undo_point.h>
#include <dpengine_test.h>
#include <parson.h>


static void dump(void **state, DP_Output *output, DP_CanvasHistory *ch,
                 const char *title)
{
    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL);
    push_canvas_state(state, cs);

    DP_output_format(output, "\n-- %s\n", title);

    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int count = DP_annotation_list_count(al);
    DP_output_format(output, "%d annotation(s)\n", count);

    for (int i = 0; i < count; ++i) {
        DP_Annotation *a = DP_annotation_list_at_noinc(al, i);
        DP_output_format(output, "[%d]\n", i);
        DP_output_format(output, "    id = %d\n", DP_annotation_id(a));
        DP_output_format(output, "    x, y, w, h = %d, %d, %d, %d\n",
                         DP_annotation_x(a), DP_annotation_y(a),
                         DP_annotation_width(a), DP_annotation_height(a));
        DP_output_format(output, "    background_color = #%08x\n",
                         DP_annotation_background_color(a));
        DP_output_format(output, "    protect = %s\n",
                         DP_annotation_protect(a) ? "true" : "false");
        DP_output_format(output, "    valign = 0x%x\n",
                         DP_annotation_valign(a));
        size_t length;
        const char *text = DP_annotation_text(a, &length);
        DP_output_format(output, "    text_length = %zu\n", length);

        JSON_Value *value = text ? json_value_init_string_with_len(text, length)
                                 : json_value_init_null();
        char *json = json_serialize_to_string_pretty(value);
        DP_output_format(output, "    text = %s\n", json);
        json_free_serialized_string(json);
        json_value_free(value);
    }

    DP_output_print(output, "\n");
    destructor_run(state, cs);
}

static void add_message(void **state, DP_Output *output, DP_CanvasHistory *ch,
                        DP_DrawContext *dc, DP_Message *msg)
{
    push_message(state, msg);
    unsigned int error_count = DP_error_count();
    bool ok = DP_canvas_history_handle(ch, dc, msg);
    const char *error = DP_error_since(error_count);
    DP_output_format(output, "-> %s %s - %u error(s)%s%s\n",
                     DP_message_type_enum_name(DP_message_type(msg)),
                     ok ? "ok" : "fail", DP_error_count_since(error_count),
                     error ? ": " : "", error ? error : "");
    destructor_run(state, msg);
}

static void add_undo_point(void **state, DP_Output *output,
                           DP_CanvasHistory *ch, DP_DrawContext *dc)
{
    add_message(state, output, ch, dc, DP_msg_undo_point_new(1));
}

static void add_undo(void **state, DP_Output *output, DP_CanvasHistory *ch,
                     DP_DrawContext *dc)
{
    add_message(state, output, ch, dc, DP_msg_undo_new(1, 0, false));
}

static void add_redo(void **state, DP_Output *output, DP_CanvasHistory *ch,
                     DP_DrawContext *dc)
{
    add_message(state, output, ch, dc, DP_msg_undo_new(1, 0, true));
}

static void add_annotation_create(void **state, DP_Output *output,
                                  DP_CanvasHistory *ch, DP_DrawContext *dc,
                                  int id, int x, int y, int width, int height)
{
    add_message(state, output, ch, dc,
                DP_msg_annotation_create_new(1, id, x, y, width, height));
}

static void add_annotation_reshape(void **state, DP_Output *output,
                                   DP_CanvasHistory *ch, DP_DrawContext *dc,
                                   int id, int x, int y, int width, int height)
{
    add_message(state, output, ch, dc,
                DP_msg_annotation_reshape_new(1, id, x, y, width, height));
}

static void add_annotation_edit(void **state, DP_Output *output,
                                DP_CanvasHistory *ch, DP_DrawContext *dc,
                                int id, uint32_t background_color, int flags,
                                const char *text)
{
    add_message(state, output, ch, dc,
                DP_msg_annotation_edit_new(1, id, background_color, flags, 0,
                                           text, text ? strlen(text) : 0));
}

static void add_annotation_delete(void **state, DP_Output *output,
                                  DP_CanvasHistory *ch, DP_DrawContext *dc,
                                  int id)
{
    add_message(state, output, ch, dc, DP_msg_annotation_delete_new(1, id));
}


static void handle_annotations(void **state)
{
    DP_Output *output =
        DP_file_output_new_from_path("test/tmp/handle_annotations");
    push_output(state, output);
    DP_output_print(output, "begin testing\n");

    DP_CanvasHistory *ch = DP_canvas_history_new();
    push_canvas_history(state, ch);
    DP_DrawContext *dc = DP_draw_context_new();
    push_draw_context(state, dc);
    dump(state, output, ch, "initial empty annotations");

    add_undo_point(state, output, ch, dc);
    add_annotation_create(state, output, ch, dc, 257, 1, 51, 101, 201);
    dump(state, output, ch, "first annotation created");

    add_undo_point(state, output, ch, dc);
    add_annotation_create(state, output, ch, dc, 257, 0, 1, 2, 3);
    dump(state, output, ch, "duplicate annotation id not created with error");

    add_undo_point(state, output, ch, dc);
    add_annotation_create(state, output, ch, dc, 258, 202, 202, 202, 202);
    dump(state, output, ch, "second annotation created");

    add_undo(state, output, ch, dc);
    dump(state, output, ch, "second annotation undone");

    add_redo(state, output, ch, dc);
    dump(state, output, ch, "second annotation redone");

    add_undo_point(state, output, ch, dc);
    add_annotation_reshape(state, output, ch, dc, 257, 101, 151, 1101, 1201);
    dump(state, output, ch, "first annotation reshaped");

    add_undo_point(state, output, ch, dc);
    add_annotation_reshape(state, output, ch, dc, 259, 0, 1, 2, 3);
    dump(state, output, ch, "unknown annotation reshaped with error");

    add_undo_point(state, output, ch, dc);
    add_annotation_edit(state, output, ch, dc, 257, 0xffffffffu,
                        DP_MSG_ANNOTATION_EDIT_PROTECT
                            | DP_MSG_ANNOTATION_EDIT_VALIGN_CENTER,
                        "first annotation");
    dump(state, output, ch, "first annotation edited");

    add_undo_point(state, output, ch, dc);
    add_annotation_edit(state, output, ch, dc, 257, 0xffabcdefu,
                        DP_MSG_ANNOTATION_EDIT_VALIGN_BOTTOM, NULL);
    dump(state, output, ch, "first annotation edited with empty text");

    add_undo_point(state, output, ch, dc);
    add_annotation_edit(state, output, ch, dc, 258, 0x0u, 0,
                        "second annotation");
    dump(state, output, ch, "second annotation edited");

    add_undo_point(state, output, ch, dc);
    add_annotation_edit(state, output, ch, dc, 259, 0x0u, 0, NULL);
    dump(state, output, ch, "nonexistent annotation edited with error");

    add_undo_point(state, output, ch, dc);
    add_annotation_delete(state, output, ch, dc, 257);
    dump(state, output, ch, "first annotation deleted");

    add_undo_point(state, output, ch, dc);
    add_annotation_delete(state, output, ch, dc, 257);
    dump(state, output, ch, "first annotation deleted again with error");

    add_undo_point(state, output, ch, dc);
    add_annotation_delete(state, output, ch, dc, 258);
    dump(state, output, ch, "second annotation deleted");

    DP_output_print(output, "done testing\n");
    destructor_run(state, output);
    assert_files_equal("test/tmp/handle_annotations",
                       "test/data/handle_annotations");
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        dp_unit_test(handle_annotations),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
