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
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/message.h>
#include <dpengine_test.h>


static void test_read_write_png(void **state)
{
    DP_Input *recording_input = DP_file_input_new_from_path(
        "test/data/recordings/transparentbackground.dprec");
    push_input(state, recording_input);

    DP_BinaryReader *reader = DP_binary_reader_new(recording_input);
    assert_non_null(reader);
    push_binary_reader(state, reader, recording_input);

    DP_CanvasHistory *ch = DP_canvas_history_new();
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
    DP_Image *write =
        DP_canvas_state_to_flat_image(cs, DP_FLAT_IMAGE_INCLUDE_BACKGROUND);
    assert_non_null(write);
    push_image(state, write);

    DP_Output *output = DP_file_output_new_from_path("test/tmp/read_write.png");
    push_output(state, output);
    if (!DP_image_write_png(write, output)) {
        DP_warn("%s", DP_error());
    }
    destructor_run(state, output);

    DP_Input *input = DP_file_input_new_from_path("test/tmp/read_write.png");
    push_input(state, input);
    DP_Image *read = DP_image_read_png(input);
    push_image(state, read);
    destructor_run(state, input);

    assert_non_null(read);
    int width = DP_image_width(write);
    assert_int_equal(width, DP_image_width(read));
    int height = DP_image_height(write);
    assert_int_equal(height, DP_image_height(read));
    size_t count = DP_int_to_size(width) * DP_int_to_size(height);
    size_t size = count * sizeof(DP_Pixel);
    assert_memory_equal(DP_image_pixels(write), DP_image_pixels(read), size);
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        dp_unit_test(test_read_write_png),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
