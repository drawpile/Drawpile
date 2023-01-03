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
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/message.h>
#include <dptest_engine.h>


static void render_recording(TEST_PARAMS)
{
    const char *name = T->test->user;
    char *dprec_path = DP_format("test/data/recordings/%s.dprec", name);
    char *out_path = DP_format("test/tmp/render_recording_%s.png", name);
    char *expected_path = DP_format("test/data/recordings/%s.png", name);

    DP_Input *input = DP_file_input_new_from_path(dprec_path);
    FATAL(NOT_NULL_OK(input, "got input for %s", dprec_path));

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    FATAL(NOT_NULL_OK(input, "got binary reader for %s", dprec_path));

    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL);
    DP_DrawContext *dc = DP_draw_context_new();

    while (true) {
        DP_Message *msg;
        DP_BinaryReaderResult result =
            DP_binary_reader_read_message(reader, &msg);
        OK(result == DP_BINARY_READER_SUCCESS
               || result == DP_BINARY_READER_INPUT_END,
           "binary read without error");

        if (result == DP_BINARY_READER_SUCCESS) {
            if (DP_message_type_command(DP_message_type(msg))) {
                if (!DP_canvas_history_handle(ch, dc, msg)) {
                    DIAG("Error handling message: %s", DP_error());
                }
            }
            DP_message_decref(msg);
        }
        else if (result == DP_BINARY_READER_INPUT_END) {
            break;
        }
        else if (result == DP_BINARY_READER_ERROR_PARSE) {
            DIAG("Parse error: %s", DP_error());
        }
        else if (result == DP_BINARY_READER_ERROR_INPUT) {
            DIAG("Input error: %s", DP_error());
            break;
        }
        else {
            DIAG("Unknown binary reader result %d", (int)result);
            break;
        }
    }

    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
    FATAL(NOT_NULL_OK(cs, "got rendered canvas state"));
    DP_draw_context_free(dc);
    DP_canvas_history_free(ch);
    DP_binary_reader_free(reader);

    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    FATAL(NOT_NULL_OK(img, "got flat image from canvas state"));
    DP_canvas_state_decref(cs);

    DP_Output *output = DP_file_output_new_from_path(out_path);
    FATAL(NOT_NULL_OK(output, "got output for %s", out_path));
    if (OK(DP_image_write_png(img, output), "wrote png to %s", out_path)) {
        IMAGE_FILE_EQ_OK(out_path, expected_path, "rendered image matches");
    }

    DP_output_free(output);
    DP_image_free(img);
    DP_free(expected_path);
    DP_free(out_path);
    DP_free(dprec_path);
}


static void register_tests(REGISTER_PARAMS)
{
    DP_test_register(REGISTER_ARGS, "brushmodes", render_recording,
                     "brushmodes");
    DP_test_register(REGISTER_ARGS, "layermodes", render_recording,
                     "layermodes");
    DP_test_register(REGISTER_ARGS, "layerops", render_recording, "layerops");
    DP_test_register(REGISTER_ARGS, "persp", render_recording, "persp");
    DP_test_register(REGISTER_ARGS, "rect", render_recording, "rect");
    DP_test_register(REGISTER_ARGS, "resize", render_recording, "resize");
    DP_test_register(REGISTER_ARGS, "transform", render_recording, "transform");
    DP_test_register(REGISTER_ARGS, "transparentbackground", render_recording,
                     "transparentbackground");
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
