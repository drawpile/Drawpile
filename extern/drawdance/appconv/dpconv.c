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
#include <dpcommon/cpu.h>
#include <dpcommon/file.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpcommon/threading.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpengine/paint_engine.h>
#include <dpengine/player.h>
#include <dpengine/recorder.h>
#include <dpengine/save.h>
#include <dpmsg/acl.h>
#include <dpmsg/message.h>
#include <dpmsg/msg_internal.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


#define MESSAGE_BUFFER_SIZE 64

typedef enum DP_ConvFormat {
    DP_CONV_FORMAT_GUESS,
    DP_CONV_FORMAT_DPREC,
    DP_CONV_FORMAT_DPTXT,
    DP_CONV_FORMAT_ORA,
    DP_CONV_FORMAT_PNG,
    DP_CONV_FORMAT_JPEG,
} DP_ConvFormat;

typedef struct DP_ConvContext {
    bool want_help;
    DP_ConvFormat input_format;
    DP_ConvFormat output_format;
    const char *input_path;
    const char *output_path;
    DP_Input *input;
    DP_Player *player;
} DP_ConvContext;


static bool format_valid_for_input(DP_ConvFormat format)
{
    switch (format) {
    case DP_CONV_FORMAT_GUESS:
    case DP_CONV_FORMAT_DPREC:
    case DP_CONV_FORMAT_DPTXT:
        return true;
    default:
        return false;
    }
}

static bool format_valid_for_output(DP_ConvFormat format)
{
    switch (format) {
    case DP_CONV_FORMAT_GUESS:
    case DP_CONV_FORMAT_DPREC:
    case DP_CONV_FORMAT_DPTXT:
    case DP_CONV_FORMAT_ORA:
    case DP_CONV_FORMAT_PNG:
    case DP_CONV_FORMAT_JPEG:
        return true;
    default:
        return false;
    }
}

static const char *format_to_string(DP_ConvFormat format)
{
    switch (format) {
    case DP_CONV_FORMAT_GUESS:
        return "guess";
    case DP_CONV_FORMAT_DPREC:
        return "dprec";
    case DP_CONV_FORMAT_DPTXT:
        return "dptxt";
    case DP_CONV_FORMAT_ORA:
        return "ora";
    case DP_CONV_FORMAT_PNG:
        return "png";
    case DP_CONV_FORMAT_JPEG:
        return "jpeg";
    default:
        return "unknown";
    }
}


static void print_usage(const char *progname, FILE *fp)
{
    int spaces = DP_size_to_int(strlen(progname));
    fprintf(fp,
            "\n"
            "Usage:\n"
            "    %s --input=INPUTFILE\n"
            "    %*c --output=OUTPUTFILE\n"
            "    %*c [--input-format=guess|dprec|dptxt]\n"
            "    %*c [--output-format=guess|dprec|dptxt|ora|png|jpg|jpeg]\n"
            "Show full help:\n"
            "    %s --help|-help|-h|-?\n"
            "\n",
            progname, spaces, ' ', spaces, ' ', spaces, ' ', progname);
}

static void print_help(const char *progname, FILE *fp)
{
    fputs("\ndpconv - convert Drawpile recordings\n", stdout);
    print_usage(progname, fp);
}

static void warnv(const char *fmt, va_list ap)
{
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

static void warn(const char *fmt, ...) DP_FORMAT(1, 2);
static void warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    warnv(fmt, ap);
    va_end(ap);
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

static bool parse_format(const char *value, DP_ConvFormat *out_format)
{
    if (DP_str_equal_lowercase(value, "guess")) {
        *out_format = DP_CONV_FORMAT_GUESS;
        return true;
    }
    else if (DP_str_equal_lowercase(value, "dprec")) {
        *out_format = DP_CONV_FORMAT_DPREC;
        return true;
    }
    else if (DP_str_equal_lowercase(value, "dptxt")) {
        *out_format = DP_CONV_FORMAT_DPTXT;
        return true;
    }
    else if (DP_str_equal_lowercase(value, "ora")) {
        *out_format = DP_CONV_FORMAT_ORA;
        return true;
    }
    else if (DP_str_equal_lowercase(value, "png")) {
        *out_format = DP_CONV_FORMAT_PNG;
        return true;
    }
    else if (DP_str_equal_lowercase(value, "jpg")
             || DP_str_equal_lowercase(value, "jpeg")) {
        *out_format = DP_CONV_FORMAT_JPEG;
        return true;
    }
    else {
        warn("Unknown format '%s'", value);
        return false;
    }
}

static bool parse_arg(DP_ConvContext *c, const char *arg)
{
    int offset;
    if (DP_str_equal_lowercase(arg, "--help")
        || DP_str_equal_lowercase(arg, "-help")
        || DP_str_equal_lowercase(arg, "-h")
        || DP_str_equal_lowercase(arg, "-?")) {
        c->want_help = true;
        return true;
    }
    else if (starts_with(arg, "--input-format=", &offset)) {
        return parse_format(arg + offset, &c->input_format);
    }
    else if (starts_with(arg, "--output-format=", &offset)) {
        return parse_format(arg + offset, &c->output_format);
    }
    else if (starts_with(arg, "--input=", &offset)) {
        c->input_path = arg + offset;
        return true;
    }
    else if (starts_with(arg, "--output=", &offset)) {
        c->output_path = arg + offset;
        return true;
    }
    else {
        warn("Unknown argument: '%s'", arg);
        return false;
    }
}

static const char *get_program_name(int argc, char **argv)
{
    return argc > 0 && argv[0] && argv[0][0] != '\0' ? argv[0] : "dpconv";
}

static void validate_arg(bool *out_ok, bool valid, const char *fmt, ...)
    DP_FORMAT(3, 4);

static void validate_arg(bool *out_ok, bool valid, const char *fmt, ...)
{
    if (!valid) {
        va_list ap;
        va_start(ap, fmt);
        warnv(fmt, ap);
        va_end(ap);
        *out_ok = false;
    }
}

static int parse_args(DP_ConvContext *c, int argc, char **argv)
{
    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        ok = parse_arg(c, argv[i]) && ok;
    }

    if (c->want_help) {
        print_help(get_program_name(argc, argv), stdout);
        return ok ? -1 : 2;
    }

    validate_arg(&ok, c->input_path, "Missing required parameter --input");
    validate_arg(&ok, c->output_path, "Missing required parameter --output");
    validate_arg(&ok, format_valid_for_input(c->input_format),
                 "Format '%s' can't be used for input",
                 format_to_string(c->input_format));
    validate_arg(&ok, format_valid_for_output(c->output_format),
                 "Format '%s' can't be used for output",
                 format_to_string(c->output_format));

    if (!ok) {
        print_usage(get_program_name(argc, argv), stderr);
    }

    return ok ? 0 : 2;
}


static bool ends_with_lowercase(const char *arg, const char *suffix)
{
    size_t arg_len = strlen(arg);
    size_t suffix_len = strlen(suffix);
    if (arg_len >= suffix_len) {
        return DP_str_equal_lowercase(arg + (arg_len - suffix_len), suffix);
    }
    else {
        return false;
    }
}

static bool guess_output_format(DP_ConvContext *c)
{
    if (c->output_format == DP_CONV_FORMAT_GUESS && c->output_path) {
        if (ends_with_lowercase(c->output_path, ".dprec")) {
            c->output_format = DP_CONV_FORMAT_DPREC;
        }
        else if (ends_with_lowercase(c->output_path, ".dptxt")) {
            c->output_format = DP_CONV_FORMAT_DPTXT;
        }
        else if (ends_with_lowercase(c->output_path, ".ora")) {
            c->output_format = DP_CONV_FORMAT_ORA;
        }
        else if (ends_with_lowercase(c->output_path, ".png")) {
            c->output_format = DP_CONV_FORMAT_PNG;
        }
        else if (ends_with_lowercase(c->output_path, ".jpg")
                 || ends_with_lowercase(c->output_path, ".jpeg")) {
            c->output_format = DP_CONV_FORMAT_JPEG;
        }
    }
    return c->output_format != DP_CONV_FORMAT_GUESS;
}

static bool open_input(DP_ConvContext *c, const char **out_recording_path)
{
    if (DP_str_equal(c->input_path, "-")) {
        *out_recording_path = NULL;
        c->input = DP_file_input_new(stdin, false);
    }
    else {
        *out_recording_path = c->input_path;
        c->input = DP_file_input_new_from_path(c->input_path);
    }
    return c->input != NULL;
}

static bool open_player(DP_ConvContext *c)
{
    DP_PlayerType type;
    switch (c->input_format) {
    case DP_CONV_FORMAT_GUESS:
        type = DP_PLAYER_TYPE_GUESS;
        break;
    case DP_CONV_FORMAT_DPREC:
        type = DP_PLAYER_TYPE_BINARY;
        break;
    case DP_CONV_FORMAT_DPTXT:
        type = DP_PLAYER_TYPE_TEXT;
        break;
    default:
        warn("Unknown input format '%s'", format_to_string(c->input_format));
        return false;
    }

    c->player = DP_player_new(type, c->input_path, c->input, NULL);
    c->input = NULL; // Taken over by the player or freed on failure.
    return c->player != NULL;
}


static int convert_recording(DP_ConvContext *c, DP_RecorderType recorder_type)
{
    DP_Output *output;
    if (DP_str_equal(c->output_path, "-")) {
        output = DP_file_output_new(stdout, false);
    }
    else {
        output = DP_file_output_new_from_path(c->output_path);
    }
    if (!output) {
        warn("Can't open '%s': %s", c->output_path, DP_error());
        return 1;
    }

    DP_Recorder *recorder =
        DP_recorder_new_inc(recorder_type, NULL, NULL, NULL, output);
    bool ok = true;

    while (ok) {
        DP_Message *msg;
        DP_PlayerResult result = DP_player_step(c->player, &msg);
        if (result == DP_PLAYER_SUCCESS) {
            ok = DP_recorder_message_push_inc(recorder, msg);
            DP_message_decref(msg);
            if (!ok) {
                warn("Error recording message: %s", DP_error());
                break;
            }
        }
        else if (result == DP_PLAYER_ERROR_PARSE) {
            warn("Error parsing message: %s", DP_error());
        }
        else {
            if (result != DP_PLAYER_RECORDING_END) {
                warn("Playback error: %s", DP_error());
                ok = false;
            }
            break;
        }
    }

    DP_recorder_free_join(recorder);
    return ok ? 0 : 1;
}


static void on_acls_changed(DP_UNUSED void *user,
                            DP_UNUSED int acl_change_flags)
{
}

static void on_laser_trail(DP_UNUSED void *user,
                           DP_UNUSED unsigned int context_id,
                           DP_UNUSED int persistence, DP_UNUSED uint32_t color)
{
}

static void on_move_pointer(DP_UNUSED void *user,
                            DP_UNUSED unsigned int context_id, DP_UNUSED int x,
                            DP_UNUSED int y)
{
}

static void on_playback(void *user, DP_UNUSED long long position,
                        DP_UNUSED int interval)
{
    DP_SEMAPHORE_MUST_POST(user);
}

static void flush_messages(DP_PaintEngine *pe, DP_Message **msgs, int fill)
{
    DP_paint_engine_handle_inc(pe, false, true, fill, msgs, on_acls_changed,
                               on_laser_trail, on_move_pointer, NULL);
    for (int i = 0; i < fill; ++i) {
        DP_message_decref(msgs[i]);
    }
}

static int render_image(DP_ConvContext *c, DP_SaveImageType save_type)
{
    DP_Semaphore *sem = DP_semaphore_new(0);
    if (!sem) {
        warn("Can't create semaphore: %s", DP_error());
        return 1;
    }

    DP_DrawContext *dc = DP_draw_context_new();
    DP_AclState *acls = DP_acl_state_new();
    DP_PaintEngine *pe =
        DP_paint_engine_new_inc(dc, NULL, acls, NULL, NULL, NULL, false, NULL,
                                NULL, NULL, NULL, on_playback, NULL, sem);

    DP_Message *msgs[MESSAGE_BUFFER_SIZE];
    int fill = 0;
    bool ok = true;

    while (true) {
        DP_PlayerResult result = DP_player_step(c->player, &msgs[fill]);
        if (result == DP_PLAYER_SUCCESS) {
            if (++fill == MESSAGE_BUFFER_SIZE) {
                flush_messages(pe, msgs, fill);
                fill = 0;
            }
        }
        else if (result == DP_PLAYER_ERROR_PARSE) {
            warn("Error parsing message: %s", DP_error());
        }
        else {
            if (result != DP_PLAYER_RECORDING_END) {
                warn("Playback error: %s", DP_error());
                ok = false;
            }
            break;
        }
    }

    msgs[fill++] = DP_msg_internal_playback_new(0, 0, 0);
    flush_messages(pe, msgs, fill);

    DP_SEMAPHORE_MUST_WAIT(sem);
    DP_semaphore_free(sem);

    DP_CanvasState *cs = DP_paint_engine_sample_canvas_state_inc(pe);
    DP_paint_engine_free_join(pe);
    DP_acl_state_free(acls);

    DP_SaveResult result = DP_save(cs, dc, save_type, c->output_path);
    DP_canvas_state_decref(cs);
    DP_draw_context_free(dc);

    switch (result) {
    case DP_SAVE_RESULT_SUCCESS:
        return ok ? 0 : 1;
    default:
        warn("Save error: %s", DP_error());
        return 1;
    }
}


static int run(DP_ConvContext *c, int argc, char **argv)
{
    int ret = parse_args(c, argc, argv);
    if (ret != 0) {
        return ret < 0 ? 0 : ret;
    }

    if (!guess_output_format(c)) {
        warn("Can't guess format for output '%s', use --output-format to "
             "specify it explicitly.",
             c->output_path);
        return 1;
    }

    const char *recording_path;
    if (!open_input(c, &recording_path)) {
        warn("Can't open input '%s': %s", c->input_path, DP_error());
        return 1;
    }

    if (!open_player(c)) {
        warn("Can't open recording '%s': %s", c->input_path, DP_error());
        return 1;
    }

    switch (c->output_format) {
    case DP_CONV_FORMAT_DPREC:
        return convert_recording(c, DP_RECORDER_TYPE_BINARY);
    case DP_CONV_FORMAT_DPTXT:
        return convert_recording(c, DP_RECORDER_TYPE_TEXT);
    case DP_CONV_FORMAT_ORA:
        return render_image(c, DP_SAVE_IMAGE_ORA);
    case DP_CONV_FORMAT_PNG:
        return render_image(c, DP_SAVE_IMAGE_PNG);
    case DP_CONV_FORMAT_JPEG:
        return render_image(c, DP_SAVE_IMAGE_JPEG);
    default:
        warn("Unknown output format '%s'", format_to_string(c->output_format));
        return 1;
    }
}

int main(int argc, char **argv)
{
    DP_cpu_support_init();
    DP_ConvContext c = {
        false, DP_CONV_FORMAT_GUESS, DP_CONV_FORMAT_GUESS, NULL, NULL, NULL,
        NULL};
    int ret = run(&c, argc, argv);
    DP_player_free(c.player);
    DP_input_free(c.input);
    return ret;
}
