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
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/binary_writer.h>
#include <dpmsg/message.h>
#include <dpmsg/text_writer.h>
#include <dpmsg_test.h>
#include <parson.h>


typedef struct TestPaths {
    const char *in_path;
    const char *out_path;
    const char *expected_path;
} TestPaths;


static void test_binary_to_binary(void **state)
{
    TestPaths *paths = initial_state(state);

    DP_Input *input = DP_file_input_new_from_path(paths->in_path);
    assert_non_null(input);
    push_input(state, input);

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    assert_non_null(reader);
    push_binary_reader(state, reader, input);

    DP_Output *output = DP_file_output_new_from_path(paths->out_path);
    assert_non_null(output);
    push_output(state, output);

    DP_BinaryWriter *writer = DP_binary_writer_new(output);
    assert_non_null(writer);
    push_binary_writer(state, writer, output);

    JSON_Object *header = DP_binary_reader_header(reader);
    assert_non_null(header);

    assert_true(DP_binary_writer_write_header(writer, header));

    unsigned int error_count = DP_error_count();
    while (DP_binary_reader_has_next(reader)) {
        DP_Message *message = DP_binary_reader_read_next(reader);
        assert_non_null(message);
        push_message(state, message);
        assert_true(DP_binary_writer_write_message(writer, message));
        destructor_run(state, message);
    }

    destructor_run(state, writer);
    assert_null(DP_error_since(error_count));
    assert_files_equal(paths->out_path, paths->expected_path);
}


static void test_binary_to_text(void **state)
{
    TestPaths *paths = initial_state(state);

    DP_Input *input = DP_file_input_new_from_path(paths->in_path);
    assert_non_null(input);
    push_input(state, input);

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    assert_non_null(reader);
    push_binary_reader(state, reader, input);

    DP_Output *output = DP_file_output_new_from_path(paths->out_path);
    assert_non_null(output);
    push_output(state, output);

    DP_TextWriter *writer = DP_text_writer_new(output);
    assert_non_null(writer);
    push_text_writer(state, writer, output);

    JSON_Object *header = DP_binary_reader_header(reader);
    assert_non_null(header);

    assert_true(DP_text_writer_write_header(writer, header));

    unsigned int error_count = DP_error_count();
    while (DP_binary_reader_has_next(reader)) {
        DP_Message *message = DP_binary_reader_read_next(reader);
        assert_non_null(message);
        push_message(state, message);
        assert_true(DP_message_write_text(message, writer));
        destructor_run(state, message);
    }

    destructor_run(state, writer);
    assert_null(DP_error_since(error_count));
    assert_files_equal(paths->out_path, paths->expected_path);
}


int main(void)
{
    TestPaths paths[] = {
        {
            "test/data/blank.dprec",
            "test/tmp/blank.dprec",
            "test/data/blank.dprec",
        },
        {
            "test/data/blank.dprec",
            "test/tmp/blank.dptxt",
            "test/data/blank.dptxt",
        },
        {
            "test/data/recordings/rect.dprec",
            "test/tmp/rect.dptxt",
            "test/data/recordings/rect.dptxt",
        },
        {
            "test/data/recordings/rect.dprec",
            "test/tmp/rect.dprec",
            "test/data/recordings/rect.dprec",
        },
        {
            "test/data/resize.dprec",
            "test/tmp/resize.dprec",
            "test/data/resize.dprec",
        },
        {
            "test/data/resize.dprec",
            "test/tmp/resize.dptxt",
            "test/data/resize.dptxt",
        },
        {
            "test/data/stroke.dprec",
            "test/tmp/stroke.dprec",
            "test/data/stroke.dprec",
        },
        {
            "test/data/stroke.dprec",
            "test/tmp/stroke.dptxt",
            "test/data/stroke.dptxt",
        },
        {
            "test/data/recordings/transform.dprec",
            "test/tmp/transform.dptxt",
            "test/data/recordings/transform.dptxt",
        },
        {
            "test/data/recordings/transform.dprec",
            "test/tmp/transform.dprec",
            "test/data/recordings/transform.dprec",
        },
        {
            "test/data/drawdabs.dprec",
            "test/tmp/drawdabs.dprec",
            "test/data/drawdabs.dprec",
        },
        {
            "test/data/drawdabs.dprec",
            "test/tmp/drawdabs.dptxt",
            "test/data/drawdabs.dptxt",
        },
    };

    struct CMUnitTest tests[DP_ARRAY_LENGTH(paths)];
    for (size_t i = 0; i < DP_ARRAY_LENGTH(paths); ++i) {
        TestPaths *p = &paths[i];
        char *name = DP_format("%s -> %s", p->in_path, p->out_path);
        CMUnitTestFunction test_func;
        if (strcmp(p->out_path + strlen(p->out_path) - 6, ".dptxt") == 0) {
            test_func = test_binary_to_text;
        }
        else {
            test_func = test_binary_to_binary;
        }
        tests[i] = (struct CMUnitTest){name, test_func, setup, teardown, p};
    }

    int exit_code = cmocka_run_group_tests(tests, NULL, NULL);
    for (size_t i = 0; i < DP_ARRAY_LENGTH(paths); ++i) {
        DP_free((char *)tests[i].name);
    }
    return exit_code;
}
