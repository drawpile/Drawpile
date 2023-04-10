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
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpmsg/message.h>
#include <dptest.h>


typedef struct DP_HandleTest {
    const char *name;
    const char *out_path;
    const char *in_path;
    void (*test_fn)(DP_Output *, DP_CanvasHistory *, DP_DrawContext *);
} DP_HandleTest;

DP_UNUSED static void run_handle_test(TEST_PARAMS)
{
    DP_HandleTest *ht = T->test->user;

    DP_Output *output = DP_file_output_new_from_path(ht->out_path);
    if (NOT_NULL_OK(output, "got output for %s", ht->out_path)) {
        DP_output_format(output, "begin testing\n");

        DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL, false, NULL);
        DP_DrawContext *dc = DP_draw_context_new();
        ht->test_fn(output, ch, dc);

        DP_output_format(output, "done testing\n");
        DP_draw_context_free(dc);
        DP_canvas_history_free(ch);
        DP_output_free(output);

        FILE_EQ_OK(ht->out_path, ht->in_path, "handle log matches");
    }
}

DP_UNUSED static void register_handle_tests(REGISTER_PARAMS)
{
    for (DP_HandleTest *ht = R->user; ht->test_fn; ++ht) {
        DP_test_register(REGISTER_ARGS, ht->name, run_handle_test, ht);
    }
}


DP_UNUSED static void add_message(DP_Output *output, DP_CanvasHistory *ch,
                                  DP_DrawContext *dc, DP_Message *msg)
{
    unsigned int error_count = DP_error_count();
    bool ok = DP_canvas_history_handle(ch, dc, msg);
    const char *error = DP_error_since(error_count);
    DP_output_format(output, "-> %s %s - %u error(s)%s%s\n",
                     DP_message_type_enum_name(DP_message_type(msg)),
                     ok ? "ok" : "fail", DP_error_count_since(error_count),
                     error ? ": " : "", error ? error : "");
    DP_message_decref(msg);
}

DP_UNUSED static void add_undo_point(DP_Output *output, DP_CanvasHistory *ch,
                                     DP_DrawContext *dc)
{
    add_message(output, ch, dc, DP_msg_undo_point_new(1));
}

DP_UNUSED static void add_undo(DP_Output *output, DP_CanvasHistory *ch,
                               DP_DrawContext *dc)
{
    add_message(output, ch, dc, DP_msg_undo_new(1, 0, false));
}

DP_UNUSED static void add_redo(DP_Output *output, DP_CanvasHistory *ch,
                               DP_DrawContext *dc)
{
    add_message(output, ch, dc, DP_msg_undo_new(1, 0, true));
}


DP_UNUSED static void write_indent(DP_Output *output, int indent)
{
    static const char *space = "    ";
    for (int i = 0; i < indent; ++i) {
        DP_output_write(output, space, strlen(space));
    }
}

static void format_indent(DP_Output *output, int indent, const char *fmt, ...)
    DP_FORMAT(3, 4);

DP_UNUSED static void format_indent(DP_Output *output, int indent,
                                    const char *fmt, ...)
{
    write_indent(output, indent);
    va_list ap;
    va_start(ap, fmt);
    DP_output_vformat(output, fmt, ap);
    va_end(ap);
}
