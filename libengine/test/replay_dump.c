#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpengine/canvas_history.h>
#include <dpengine/draw_context.h>
#include <dpengine/save.h>
#include <dpmsg/message.h>

#define DUMP_TYPE_REMOTE_MESSAGE                            0
#define DUMP_TYPE_REMOTE_MESSAGE_LOCAL_DRAWING_IN_PROGRESS  1
#define DUMP_TYPE_LOCAL_MESSAGE                             2
#define DUMP_TYPE_REMOTE_MULTIDAB                           3
#define DUMP_TYPE_REMOTE_MULTIDAB_LOCAL_DRAWING_IN_PROGRESS 4
#define DUMP_TYPE_LOCAL_MULTIDAB                            5
#define DUMP_TYPE_RESET                                     6
#define DUMP_TYPE_SOFT_RESET                                7
#define DUMP_TYPE_CLEANUP                                   8


typedef struct DP_ReplayDumpContext {
    DP_Input *input;
    DP_CanvasHistory *ch;
    DP_DrawContext *dc;
    bool error;
    struct {
        size_t size;
        unsigned char *buffer;
    } read;
    struct {
        int type;
        bool local;
        bool multidab;
        bool local_drawing_in_progress;
        int capacity;
        int used;
        DP_Message **buffer;
    } message;
} DP_ReplayDumpContext;

static bool read_dump_message(DP_ReplayDumpContext *c)
{
    if (DP_input_read(c->input, c->read.buffer, 4, &c->error) != 4) {
        DP_warn("Error reading message size: %s", DP_error());
        return false;
    }

    size_t size = DP_read_bigendian_uint32(c->read.buffer);
    if (size > c->read.size) {
        c->read.buffer = DP_realloc(c->read.buffer, size);
        c->read.size = size;
    }

    if (DP_input_read(c->input, c->read.buffer, size, &c->error) != size) {
        DP_warn("Error reading message data: %s", DP_error());
        return false;
    }

    DP_Message *msg = DP_message_deserialize(c->read.buffer, size);
    if (!msg) {
        DP_warn("Error deserializing message: %s", DP_error());
        return false;
    }

    int used = c->message.used++;
    if (used == c->message.capacity) {
        int new_capacity = c->message.capacity * 2;
        c->message.buffer =
            DP_realloc(c->message.buffer, sizeof(*c->message.buffer)
                                              * DP_int_to_size(new_capacity));
        c->message.capacity = new_capacity;
    }
    c->message.buffer[used] = msg;

    return true;
}

static bool read_dump_message_entry(DP_ReplayDumpContext *c, bool local,
                                    bool local_drawing_in_progress)
{
    c->message.local = local;
    c->message.multidab = false;
    c->message.local_drawing_in_progress = local_drawing_in_progress;
    return read_dump_message(c);
}

static bool read_dump_multidab_entry(DP_ReplayDumpContext *c, bool local,
                                     bool local_drawing_in_progress)
{
    if (DP_input_read(c->input, c->read.buffer, 4, &c->error) != 4) {
        DP_warn("Failed to read multidab count: %s", DP_error());
        return false;
    }

    c->message.local = local;
    c->message.multidab = true;
    c->message.local_drawing_in_progress = local_drawing_in_progress;

    uint32_t count = DP_read_bigendian_uint32(c->read.buffer);
    for (uint32_t i = 0; i < count; ++i) {
        if (!read_dump_message(c)) {
            return false;
        }
    }
    return true;
}

static bool read_dump_entry(DP_ReplayDumpContext *c)
{
    unsigned char type;
    if (DP_input_read(c->input, &type, 1, &c->error) != 1) {
        if (c->error) {
            DP_warn("Failed to read dump entry type: %s", DP_error());
        }
        return false;
    }

    c->message.type = type;
    switch (type) {
    case DUMP_TYPE_REMOTE_MESSAGE:
        return read_dump_message_entry(c, false, false);
    case DUMP_TYPE_REMOTE_MESSAGE_LOCAL_DRAWING_IN_PROGRESS:
        return read_dump_message_entry(c, false, false);
    case DUMP_TYPE_LOCAL_MESSAGE:
        return read_dump_message_entry(c, true, false);
    case DUMP_TYPE_REMOTE_MULTIDAB:
        return read_dump_multidab_entry(c, false, false);
    case DUMP_TYPE_REMOTE_MULTIDAB_LOCAL_DRAWING_IN_PROGRESS:
        return read_dump_multidab_entry(c, false, true);
    case DUMP_TYPE_LOCAL_MULTIDAB:
        return read_dump_multidab_entry(c, true, false);
    case DUMP_TYPE_RESET:
    case DUMP_TYPE_SOFT_RESET:
    case DUMP_TYPE_CLEANUP:
        return true;
    default:
        DP_warn("Unknown dump entry type %d", DP_uchar_to_int(type));
        c->error = true;
        return false;
    }
}

static void handle_dump_entry(DP_ReplayDumpContext *c)
{
    if (c->message.type == DUMP_TYPE_RESET) {
        DP_canvas_history_reset(c->ch);
    }
    else if (c->message.type == DUMP_TYPE_SOFT_RESET) {
        DP_canvas_history_soft_reset(c->ch);
    }
    else if (c->message.type == DUMP_TYPE_CLEANUP) {
        DP_canvas_history_cleanup(c->ch, c->dc);
    }
    else if (c->message.multidab) {
        DP_canvas_history_local_drawing_in_progress_set(
            c->ch, c->message.local_drawing_in_progress);
        int count = c->message.used;
        DP_Message **msgs = c->message.buffer;
        if (c->message.local) {
            DP_canvas_history_handle_local_multidab_dec(c->ch, c->dc, count,
                                                        msgs);
        }
        else {
            DP_canvas_history_handle_multidab_dec(c->ch, c->dc, count, msgs);
        }
    }
    else {
        DP_canvas_history_local_drawing_in_progress_set(
            c->ch, c->message.local_drawing_in_progress);
        DP_Message *msg = c->message.buffer[0];
        if (c->message.local) {
            DP_canvas_history_handle_local(c->ch, c->dc, msg);
        }
        else {
            DP_canvas_history_handle(c->ch, c->dc, msg);
        }
        DP_message_decref(msg);
    }
    c->message.used = 0;
}

int main(void)
{
    const char *path = "dumps/1669117788_00000000089cac30.drawdancedump";
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        DP_warn("Can't open '%s': %s", path, DP_error());
        return EXIT_FAILURE;
    }

    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL);
    DP_DrawContext *dc = DP_draw_context_new();
    DP_ReplayDumpContext c = {
        input,
        ch,
        dc,
        false,
        {1024, DP_malloc(1024)},
        {0, false, false, false, 1024, 0,
         DP_malloc(sizeof(*c.message.buffer) * 1024)},
    };

    while (read_dump_entry(&c)) {
        handle_dump_entry(&c);
    }

    DP_CanvasState *cs = DP_canvas_history_compare_and_get(ch, NULL, NULL);
    if (DP_save(cs, dc, "dump.png") != DP_SAVE_RESULT_SUCCESS) {
        DP_warn("Error saving dump: %s", DP_error());
    }
    DP_canvas_state_decref(cs);

    DP_free(c.message.buffer);
    DP_free(c.read.buffer);
    DP_draw_context_free(dc);
    DP_canvas_history_free(ch);
    DP_input_free(input);
    return c.error ? EXIT_FAILURE : EXIT_SUCCESS;
}
