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
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/message.h>
#include <dpengine_test.h>


static void test_render_recording(void **state)
{
    const char *name = initial_state(state);
    char *dprec_path =
        push_format(state, "test/data/recordings/%s.dprec", name);
    char *out_path =
        push_format(state, "test/tmp/render_recording_%s.png", name);
    char *expected_path =
        push_format(state, "test/data/recordings/%s.png", name);

    DP_Input *input = DP_file_input_new_from_path(dprec_path);
    push_input(state, input);

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    assert_non_null(reader);
    push_binary_reader(state, reader, input);

    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL);
    assert_non_null(ch);
    push_canvas_history(state, ch);

    DP_DrawContext *dc = DP_draw_context_new();
    assert_non_null(dc);
    push_draw_context(state, dc);

    while (DP_binary_reader_has_next(reader)) {
        DP_Message *msg = DP_binary_reader_read_next(reader);
        assert_non_null(msg);
        push_message(state, msg);

        if (DP_message_type_command(DP_message_type(msg))) {
            if (!DP_canvas_history_handle(ch, dc, msg)) {
                DP_warn("%s", DP_error());
            }
        }

        destructor_run(state, msg);
    }

    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL);
    push_canvas_state(state, cs);
    DP_Image *img =
        DP_canvas_state_to_flat_image(cs, DP_FLAT_IMAGE_INCLUDE_BACKGROUND);
    assert_non_null(img);
    push_image(state, img);

    DP_Output *output = DP_file_output_new_from_path(out_path);
    push_output(state, output);
    if (!DP_image_write_png(img, output)) {
        DP_warn("%s", DP_error());
    }
    destructor_run(state, output);

    assert_image_files_equal(state, out_path, expected_path);
}


#define recording_unit_test(NAME)                          \
    (struct CMUnitTest)                                    \
    {                                                      \
        NAME, test_render_recording, setup, teardown, NAME \
    }

int main(void)
{
    const struct CMUnitTest tests[] = {
        recording_unit_test("brushmodes"),
        recording_unit_test("layermodes"),
        recording_unit_test("layerops"),
        recording_unit_test("persp"),
        recording_unit_test("rect"),
        recording_unit_test("resize"),
        recording_unit_test("transform"),
        recording_unit_test("transparentbackground"),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
