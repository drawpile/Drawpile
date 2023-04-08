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
#include <dpengine/annotation.h>
#include <dpengine/annotation_list.h>
#include <parson.h>


static const char *dump_valign(int valign)
{
    switch (valign) {
    case DP_ANNOTATION_VALIGN_TOP:
        return "top";
    case DP_ANNOTATION_VALIGN_CENTER:
        return "center";
    case DP_ANNOTATION_VALIGN_BOTTOM:
        return "bottom";
    default:
        return "unknown";
    }
}

static void dump(DP_Output *output, DP_CanvasHistory *ch, const char *title)
{
    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
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
        DP_output_format(output, "    valign = %s\n",
                         dump_valign(DP_annotation_valign(a)));
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
    DP_canvas_state_decref(cs);
}

static void add_annotation_create(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t id, int32_t x,
                                  int32_t y, uint16_t width, uint16_t height)
{
    add_message(output, ch, dc,
                DP_msg_annotation_create_new(1, id, x, y, width, height));
}

static void add_annotation_reshape(DP_Output *output, DP_CanvasHistory *ch,
                                   DP_DrawContext *dc, uint16_t id, int32_t x,
                                   int32_t y, uint16_t width, uint16_t height)
{
    add_message(output, ch, dc,
                DP_msg_annotation_reshape_new(1, id, x, y, width, height));
}

static void add_annotation_edit(DP_Output *output, DP_CanvasHistory *ch,
                                DP_DrawContext *dc, uint16_t id,
                                uint32_t background_color, uint8_t flags,
                                const char *text)
{
    add_message(output, ch, dc,
                DP_msg_annotation_edit_new(1, id, background_color, flags, 0,
                                           text, text ? strlen(text) : 0));
}

static void add_annotation_delete(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, uint16_t id)
{
    add_message(output, ch, dc, DP_msg_annotation_delete_new(1, id));
}


static void handle_annotations(DP_Output *output, DP_CanvasHistory *ch,
                               DP_DrawContext *dc)
{
    dump(output, ch, "initial empty annotations");

    add_undo_point(output, ch, dc);
    add_annotation_create(output, ch, dc, 257, 1, 51, 101, 201);
    dump(output, ch, "first annotation created");

    add_undo_point(output, ch, dc);
    add_annotation_create(output, ch, dc, 257, 0, 1, 2, 3);
    dump(output, ch, "duplicate annotation id not created with error");

    add_undo_point(output, ch, dc);
    add_annotation_create(output, ch, dc, 258, 202, 202, 202, 202);
    dump(output, ch, "second annotation created");

    add_undo(output, ch, dc);
    dump(output, ch, "second annotation undone");

    add_redo(output, ch, dc);
    dump(output, ch, "second annotation redone");

    add_undo_point(output, ch, dc);
    add_annotation_reshape(output, ch, dc, 257, 101, 151, 1101, 1201);
    dump(output, ch, "first annotation reshaped");

    add_undo_point(output, ch, dc);
    add_annotation_reshape(output, ch, dc, 259, 0, 1, 2, 3);
    dump(output, ch, "unknown annotation reshaped with error");

    add_undo_point(output, ch, dc);
    add_annotation_edit(output, ch, dc, 257, 0xffffffffu,
                        DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT
                            | DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER,
                        "first annotation");
    dump(output, ch, "first annotation edited");

    add_undo_point(output, ch, dc);
    add_annotation_edit(output, ch, dc, 257, 0xffabcdefu,
                        DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_BOTTOM, NULL);
    dump(output, ch, "first annotation edited with empty text");

    add_undo_point(output, ch, dc);
    add_annotation_edit(output, ch, dc, 258, 0x0u, 0, "second annotation");
    dump(output, ch, "second annotation edited");

    add_undo_point(output, ch, dc);
    add_annotation_edit(output, ch, dc, 259, 0x0u, 0, NULL);
    dump(output, ch, "nonexistent annotation edited with error");

    add_undo_point(output, ch, dc);
    add_annotation_delete(output, ch, dc, 257);
    dump(output, ch, "first annotation deleted");

    add_undo_point(output, ch, dc);
    add_annotation_delete(output, ch, dc, 257);
    dump(output, ch, "first annotation deleted again with error");

    add_undo_point(output, ch, dc);
    add_annotation_delete(output, ch, dc, 258);
    dump(output, ch, "second annotation deleted");
}


int main(int argc, char **argv)
{
    static DP_HandleTest tests[] = {
        {"handle_annotations", "test/tmp/handle_annotations",
         "test/data/handle_annotations", handle_annotations},
        {NULL, NULL, NULL, NULL},
    };
    return DP_test_main(argc, argv, register_handle_tests, tests);
}
