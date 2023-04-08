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


static DP_CanvasState *play_recording(TEST_PARAMS, const char *path)
{

    DP_Input *recording_input = DP_file_input_new_from_path(path);
    FATAL(NOT_NULL_OK(recording_input, "got input from %s", path));

    DP_BinaryReader *reader = DP_binary_reader_new(recording_input);
    FATAL(NOT_NULL_OK(reader, "got reader from %s", path));

    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL, false, NULL);
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
    FATAL(NOT_NULL_OK(cs, "got canvas state from playing back %s", path));
    DP_draw_context_free(dc);
    DP_canvas_history_free(ch);
    DP_binary_reader_free(reader);
    return cs;
}

static DP_Image *render_recording(TEST_PARAMS, const char *path)
{
    DP_CanvasState *cs = play_recording(TEST_ARGS, path);
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    FATAL(NOT_NULL_OK(img, "rendered image from playing back %s", path));
    DP_canvas_state_decref(cs);
    return img;
}

static bool write_png(TEST_PARAMS, DP_Image *img, const char *path)
{
    DP_Output *output = DP_file_output_new_from_path(path);
    FATAL(NOT_NULL_OK(output, "got output for %s", path));
    bool result = OK(DP_image_write_png(img, output), "write png to %s", path);
    DP_output_free(output);
    return result;
}

static DP_Image *read_png(TEST_PARAMS, const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    FATAL(NOT_NULL_OK(input, "got input for %s", path));
    DP_Image *read = DP_image_read_png(input);
    NOT_NULL_OK(read, "read png from %s", path);
    DP_input_free(input);
    return read;
}

static void read_write_png(TEST_PARAMS)
{
    const char *in_path = "test/data/recordings/transparentbackground.dprec";
    const char *out_path = "test/tmp/read_write.png";
    DP_Image *write = render_recording(TEST_ARGS, in_path);
    if (write_png(TEST_ARGS, write, out_path), "write to %s") {
        DP_Image *read = read_png(TEST_ARGS, out_path);
        if (read) {
            IMAGE_EQ_OK(write, read, "reading back png matches");
            DP_image_free(read);
        }
    }
    DP_image_free(write);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(read_write_png);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
