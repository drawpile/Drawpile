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
#include "dpcommon/conversions.h"
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/message.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


typedef enum DP_ConvFormat {
    DP_CONV_FORMAT_GUESS,
    DP_CONV_FORMAT_DPREC,
    DP_CONV_FORMAT_DPTXT,
    DP_CONV_FORMAT_ORA,
    DP_CONV_FORMAT_PNG,
    DP_CONV_FORMAT_JPG,
} DP_ConvFormat;

typedef struct DP_ConvParams {
    bool want_help;
    DP_ConvFormat input_format;
    DP_ConvFormat output_format;
    const char *input;
    const char *output;
} DP_ConvParams;


static void print_usage(const char *progname)
{
    int spaces = DP_size_to_int(strlen(progname));
    fprintf(stderr,
            "\n"
            "Usage:\n"
            "    %s [--input=INPUTFILE] \\\n"
            "    %*c [--output=OUTPUTFILE] \\\n"
            "    %*c [--input-format=guess|dprec|dptxt] \\\n"
            "    %*c [--output-format=guess|dprec|dptxt|ora|png|jpg|jpeg]\n"
            "Show full help:\n"
            "    %s --help|-help|-h|-?\n"
            "\n",
            progname, spaces, ' ', spaces, ' ', spaces, ' ', progname);
}

static void print_help(void)
{
    fputs("dpconv - convert Drawpile recordings\n", stdout);
}

static void warn(const char *fmt, ...) DP_FORMAT(1, 2);
static void warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}


static bool eq_ignore_case(const char *a, const char *b)
{
    size_t len = strlen(a);
    if (len != strlen(b)) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (tolower(a[i]) != tolower(b[i])) {
            return false;
        }
    }
    return true;
}

static bool starts_with(const char *arg, const char *prefix, int *offset)
{
    if (strncmp(arg, prefix, strlen(prefix)) == 0) {
        *offset = DP_size_to_int(strlen(prefix));
        return true;
    }
    else {
        return false;
    }
}

static bool parse_input_format(DP_ConvParams *params, const char *format)
{
    if (eq_ignore_case(format, "guess")) {
        params->input_format = DP_CONV_FORMAT_GUESS;
        return true;
    }
    else if (eq_ignore_case(format, "dprec")) {
        params->input_format = DP_CONV_FORMAT_DPREC;
        return true;
    }
    else {
        warn("Unknown input format '%s'", format);
        return false;
    }
}

static bool parse_output_format(DP_ConvParams *params, const char *format)
{
    if (eq_ignore_case(format, "guess")) {
        params->output_format = DP_CONV_FORMAT_GUESS;
        return true;
    }
    else if (eq_ignore_case(format, "png")) {
        params->output_format = DP_CONV_FORMAT_PNG;
        return true;
    }
    else {
        warn("Unknown output format '%s'", format);
        return false;
    }
}

static bool parse_arg(DP_ConvParams *params, const char *arg)
{
    int offset;
    if (eq_ignore_case(arg, "--help") || eq_ignore_case(arg, "-help")
        || eq_ignore_case(arg, "-h") || eq_ignore_case(arg, "-?")) {
        params->want_help = true;
        return true;
    }
    else if (starts_with(arg, "--input-format=", &offset)) {
        return parse_input_format(params, arg + offset);
    }
    else if (starts_with(arg, "--output-format=", &offset)) {
        return parse_output_format(params, arg + offset);
    }
    else if (starts_with(arg, "--input=", &offset)) {
        params->input = arg + offset;
        return true;
    }
    else if (starts_with(arg, "--output=", &offset)) {
        params->output = arg + offset;
        return true;
    }
    else {
        warn("Unknown argument: '%s'", arg);
        return false;
    }
}

static int parse_args(DP_ConvParams *params, int argc, char **argv)
{
    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        ok = parse_arg(params, argv[i]) && ok;
    }

    if (!ok) {
        const char *progname = argc > 0 ? argv[0] : "dpconv";
        print_usage(progname);
    }

    if (params->want_help) {
        print_help();
    }

    if (ok) {
        return params->want_help ? -1 : 0;
    }
    else {
        return 2;
    }
}


static DP_Input *open_input(const char *path)
{
    if (!path || eq_ignore_case(path, "-")) {
        return DP_file_input_new(stdin, false);
    }
    else {
        return DP_file_input_new_from_path(path);
    }
}

static DP_Output *open_output(const char *path)
{
    if (!path || eq_ignore_case(path, "-")) {
        return DP_file_output_new(stdout, false);
    }
    else {
        return DP_file_output_new_from_path(path);
    }
}

int main(int argc, char **argv)
{
    DP_ConvParams params = {false, DP_CONV_FORMAT_GUESS, DP_CONV_FORMAT_GUESS,
                            NULL, NULL};
    int ret = parse_args(&params, argc, argv);
    if (ret != 0) {
        return ret < 0 ? 0 : ret;
    }

    DP_Input *input = open_input(params.input);
    if (!input) {
        warn("Can't open input '%s': %s", params.input, DP_error());
        return 1;
    }

    DP_Output *output = open_output(params.output);
    if (!output) {
        warn("Can't open output '%s': %s", params.output, DP_error());
        DP_input_free(input);
        return 1;
    }

    DP_BinaryReader *reader = DP_binary_reader_new(input);
    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL);
    DP_DrawContext *dc = DP_draw_context_new();

    while (DP_binary_reader_has_next(reader)) {
        DP_Message *msg = DP_binary_reader_read_next(reader);
        if (!msg) {
            warn("Read: %s", DP_error());
            continue;
        }

        if (DP_message_type_command(DP_message_type(msg))) {
            if (!DP_canvas_history_handle(ch, dc, msg)) {
                warn("Handle: %s", DP_error());
            }
        }

        DP_message_decref(msg);
    }

    // TODO error
    DP_binary_reader_free(reader);

    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    DP_canvas_state_decref(cs);

    DP_draw_context_free(dc);
    DP_canvas_history_free(ch);
    if (!DP_image_write_png(img, output)) {
        warn("Couldn't write PNG: %s", DP_error());
    }
    // TODO error
    DP_output_free(output);
    DP_image_free(img);
    return 0;
}
