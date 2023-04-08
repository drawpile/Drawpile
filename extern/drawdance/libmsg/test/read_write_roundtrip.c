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
#include <dpmsg/binary_reader.h>
#include <dpmsg/binary_writer.h>
#include <dpmsg/message.h>
#include <dpmsg/text_writer.h>
#include <dptest.h>
#include <parson.h>


static void write_message_binary(TEST_PARAMS, void *writer, DP_Message *msg)
{
    OK(DP_binary_writer_write_message(writer, msg), "wrote message to binary");
}

static void write_message_text(TEST_PARAMS, void *writer, DP_Message *msg)
{
    OK(DP_message_write_text(msg, writer), "wrote message to text");
}

static void read_write_binary(TEST_PARAMS, DP_BinaryReader *reader,
                              void (*write_message)(TEST_PARAMS, void *,
                                                    DP_Message *),
                              void *writer)
{
    while (true) {
        DP_Message *msg;
        DP_BinaryReaderResult result =
            DP_binary_reader_read_message(reader, &msg);
        OK(result == DP_BINARY_READER_SUCCESS
               || result == DP_BINARY_READER_INPUT_END,
           "binary read without error");

        if (result == DP_BINARY_READER_SUCCESS) {
            write_message(TEST_ARGS, writer, msg);
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
}

static void binary_to_binary(TEST_PARAMS)
{
    const char *key = T->test->user;
    char *in_path = DP_format("test/data/recordings/%s.dprec", key);
    char *out_path =
        DP_format("test/tmp/read_binary_write_binary%s.dprec", key);

    DP_Input *input = DP_file_input_new_from_path(in_path);
    FATAL(NOT_NULL_OK(input, "got input for %s", in_path));

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    FATAL(NOT_NULL_OK(reader, "got binary reader for %s", in_path));

    DP_Output *output = DP_file_output_new_from_path(out_path);
    FATAL(NOT_NULL_OK(reader, "got output for %s", out_path));

    DP_BinaryWriter *writer = DP_binary_writer_new(output);
    FATAL(NOT_NULL_OK(writer, "got binary writer for %s", out_path));

    JSON_Object *header = DP_binary_reader_header(reader);
    if (NOT_NULL_OK(header, "got binary reader header")) {
        OK(DP_binary_writer_write_header(writer, header), "wrote header");
    }

    read_write_binary(TEST_ARGS, reader, write_message_binary, writer);

    DP_binary_writer_free(writer);
    DP_binary_reader_free(reader);

    FILE_EQ_OK(out_path, in_path, "binary output equal");

    DP_free(out_path);
    DP_free(in_path);
}


static void binary_to_text(TEST_PARAMS)
{
    const char *key = T->test->user;
    char *in_path = DP_format("test/data/recordings/%s.dprec", key);
    char *out_path = DP_format("test/tmp/read_binary_write_text_%s.dprec", key);
    char *expected_path = DP_format("test/data/recordings/%s.dptxt", key);

    DP_Input *input = DP_file_input_new_from_path(in_path);
    FATAL(NOT_NULL_OK(input, "got input for %s", in_path));

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    FATAL(NOT_NULL_OK(reader, "got binary reader for %s", in_path));

    DP_Output *output = DP_file_output_new_from_path(out_path);
    FATAL(NOT_NULL_OK(reader, "got output for %s", out_path));

    DP_TextWriter *writer = DP_text_writer_new(output);
    FATAL(NOT_NULL_OK(writer, "got text writer for %s", out_path));

    JSON_Object *header = DP_binary_reader_header(reader);
    if (NOT_NULL_OK(header, "got binary reader header")) {
        OK(DP_text_writer_write_header(writer, header), "wrote header");
    }

    read_write_binary(TEST_ARGS, reader, write_message_text, writer);

    DP_text_writer_free(writer);
    DP_binary_reader_free(reader);

    FILE_EQ_OK(out_path, expected_path, "text output equal");

    DP_free(expected_path);
    DP_free(out_path);
    DP_free(in_path);
}


static void register_tests(REGISTER_PARAMS)
{
    const char *keys[] = {
        "blank", "rect", "resize", "stroke", "transform", "drawdabs",
    };

    for (size_t i = 0; i < DP_ARRAY_LENGTH(keys); ++i) {
        const char *key = keys[i];
        {
            char *binary_name = DP_format("%s_binary", key);
            DP_test_register(REGISTER_ARGS, binary_name, binary_to_binary,
                             (void *)key);
            DP_free(binary_name);
        }
        {
            char *text_name = DP_format("%s_text", key);
            DP_test_register(REGISTER_ARGS, text_name, binary_to_text,
                             (void *)key);
            DP_free(text_name);
        }
    }
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
