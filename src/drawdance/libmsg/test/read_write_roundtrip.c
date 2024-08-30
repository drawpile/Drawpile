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
#include <dpcommon/binary.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpcommon/vector.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/binary_writer.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>
#include <dpmsg/message_queue.h>
#include <dpmsg/reset_stream.h>
#include <dpmsg/text_reader.h>
#include <dpmsg/text_writer.h>
#include <dptest.h>
#include <limits.h>
#include <parson.h>


static DP_BinaryWriter *open_binary_writer(TEST_PARAMS, const char *path)
{
    DP_Output *bo = DP_file_output_new_from_path(path);
    FATAL(NOT_NULL_OK(bo, "got binary output for %s", path));
    DP_BinaryWriter *bw = DP_binary_writer_new(bo);
    FATAL(NOT_NULL_OK(bw, "got binary writer for %s", path));
    return bw;
}

static DP_TextWriter *open_text_writer(TEST_PARAMS, const char *path)
{
    DP_Output *bo = DP_file_output_new_from_path(path);
    FATAL(NOT_NULL_OK(bo, "got text output for %s", path));
    DP_TextWriter *tw = DP_text_writer_new(bo);
    FATAL(NOT_NULL_OK(tw, "got text writer for %s", path));
    return tw;
}

static DP_BinaryReader *open_binary_reader(TEST_PARAMS, const char *path)
{
    DP_Input *bi = DP_file_input_new_from_path(path);
    FATAL(NOT_NULL_OK(bi, "got binary input for %s", path));
    DP_BinaryReader *br = DP_binary_reader_new(bi, 0);
    FATAL(NOT_NULL_OK(br, "got binary reader for %s", path));
    return br;
}

static DP_TextReader *open_text_reader(TEST_PARAMS, const char *path)
{
    DP_Input *ti = DP_file_input_new_from_path(path);
    FATAL(NOT_NULL_OK(ti, "got text input for %s", path));
    DP_TextReader *tr = DP_text_reader_new(ti);
    FATAL(NOT_NULL_OK(tr, "got text reader for %s", path));
    return tr;
}

static void write_header(TEST_PARAMS, DP_BinaryWriter *bw, DP_TextWriter *tw,
                         JSON_Object *header)
{
    OK(DP_binary_writer_write_header(bw, header), "wrote binary header");
    OK(DP_text_writer_write_header(tw, header), "wrote text header");
}

static void write_initial_header(TEST_PARAMS, DP_BinaryWriter *bw,
                                 DP_TextWriter *tw)
{
    JSON_Value *header_value = json_value_init_object();
    JSON_Object *header = json_value_get_object(header_value);
    json_object_set_string(header, "version", DP_PROTOCOL_VERSION);
    json_object_set_string(header, "writer", "read_write_roundtrip");
    json_object_set_string(header, "type", "recording");
    write_header(TEST_ARGS, bw, tw, header);
    json_value_free(header_value);
}

static void write_message_binary(TEST_PARAMS, DP_Message *msg,
                                 DP_BinaryWriter *bw)
{
    OK(DP_binary_writer_write_message(bw, msg) != 0, "wrote message to binary");
}

static void write_message_text(TEST_PARAMS, DP_Message *msg, DP_TextWriter *tw)
{
    OK(DP_message_write_text(msg, tw), "wrote message to text");
}


static bool random_bool(void)
{
    return rand() % 2 == 0;
}

static uint32_t random_uint32(void)
{
    unsigned char bytes[] = {
        DP_int_to_uchar(rand() & 0xff),
        DP_int_to_uchar(rand() & 0xff),
        DP_int_to_uchar(rand() & 0xff),
        DP_int_to_uchar(rand() & 0xff),
    };
    return DP_read_littleendian_uint32(bytes);
}

static uint16_t random_uint16(void)
{
    return DP_uint32_to_uint16(random_uint32() & (uint32_t)0xffffu);
}

static uint8_t random_uint8(void)
{
    return DP_uint32_to_uint8(random_uint32() & (uint32_t)0xffu);
}

static int8_t random_int8(void)
{
    return DP_uint8_to_int8(random_uint8());
}

static int32_t random_int32(void)
{
    return DP_uint32_to_int32(random_uint32());
}

static int int_between(int min_inclusive, int max_inclusive)
{
    DP_ASSERT(min_inclusive <= max_inclusive);
    DP_ASSERT(min_inclusive >= 0);
    return min_inclusive
         + DP_uint32_to_int(
               random_uint32()
               % DP_int_to_uint32(max_inclusive - min_inclusive + 1));
}

static size_t size_between(size_t min_inclusive, size_t max_inclusive)
{
    DP_ASSERT(min_inclusive <= max_inclusive);
    DP_ASSERT(max_inclusive <= UINT32_MAX);
    return min_inclusive
         + DP_uint32_to_size(
               random_uint32()
               % DP_size_to_uint32(max_inclusive - min_inclusive + (size_t)1));
}

static unsigned int generate_context_id(void)
{
    return random_uint8();
}

static uint8_t generate_blend_mode(void)
{
    int mode = int_between(0, DP_BLEND_MODE_LAST_EXCEPT_REPLACE);
    if (mode == DP_BLEND_MODE_LAST_EXCEPT_REPLACE) {
        return DP_BLEND_MODE_REPLACE;
    }
    else {
        return DP_int_to_uint8(mode);
    }
}

static uint8_t generate_flags(const unsigned int *flag_list, int count)
{
    uint8_t flags = 0;
    for (int i = 0; i < count; ++i) {
        unsigned int flag = flag_list[i];
        DP_ASSERT(flag < UINT8_MAX);
        if (random_bool()) {
            flags |= DP_uint_to_uint8(flag);
        }
    }
    return flags;
}

static uint8_t generate_variant(const unsigned int *variant_list, int count)
{
    unsigned int variant = variant_list[int_between(0, count - 1)];
    DP_ASSERT(variant <= UINT8_MAX);
    return DP_uint_to_uint8(variant);
}

static char *generate_string(size_t length, size_t *out_length)
{
    char *string = DP_malloc(length + 1);
    for (size_t i = 0; i < length; ++i) {
        // NUL can't appear in any strings. Carriage return doesn't round-trip
        // correctly in text mode, which is inconsequential in practice, but of
        // course causes these tests to fail. So we exclude it.
        char c;
        do {
            c = DP_uint8_to_char(random_uint8());
        } while (c == '\0' || c == '\r');
        string[i] = c;
    }
    string[length] = '\0';
    *out_length = length;
    return string;
}

static void generate_bytes(size_t size, unsigned char *out,
                           DP_UNUSED void *user)
{
    for (size_t i = 0; i < size; ++i) {
        out[i] = random_uint8();
    }
}

static void generate_uint8s(int count, uint8_t *out, DP_UNUSED void *user)
{
    for (int i = 0; i < count; ++i) {
        out[i] = random_uint8();
    }
}

static void generate_uint16s(int count, uint16_t *out, DP_UNUSED void *user)
{
    for (int i = 0; i < count; ++i) {
        out[i] = random_uint16();
    }
}

static void generate_classic_dabs(int count, DP_ClassicDab *out,
                                  DP_UNUSED void *user)
{
    for (int i = 0; i < count; ++i) {
        DP_classic_dab_init(out, i, random_int8(), random_int8(),
                            random_uint16(), random_uint8(), random_uint8());
    }
}

static void generate_pixel_dabs(int count, DP_PixelDab *out,
                                DP_UNUSED void *user)
{
    for (int i = 0; i < count; ++i) {
        DP_pixel_dab_init(out, i, random_int8(), random_int8(), random_uint8(),
                          random_uint8());
    }
}

static void generate_mypaint_dabs(int count, DP_MyPaintDab *out,
                                  DP_UNUSED void *user)
{
    for (int i = 0; i < count; ++i) {
        DP_mypaint_dab_init(out, i, random_int8(), random_int8(),
                            random_uint16(), random_uint8(), random_uint8(),
                            random_uint8(), random_uint8());
    }
}

static DP_Message *generate_server_command(void)
{
    size_t message_len;
    char *message =
        generate_string(size_between(DP_MSG_SERVER_COMMAND_MSG_MIN_LEN,
                                     DP_MSG_SERVER_COMMAND_MSG_MAX_LEN),
                        &message_len);
    DP_Message *msg =
        DP_msg_server_command_new(generate_context_id(), message, message_len);
    DP_free(message);
    return msg;
}

static DP_Message *generate_ping(void)
{
    return DP_msg_ping_new(generate_context_id(), random_bool());
}

static DP_Message *generate_disconnect(void)
{
    size_t message_len;
    char *message =
        generate_string(size_between(DP_MSG_DISCONNECT_MESSAGE_MIN_LEN,
                                     DP_MSG_DISCONNECT_MESSAGE_MAX_LEN),
                        &message_len);
    DP_Message *msg = DP_msg_disconnect_new(
        generate_context_id(),
        generate_variant((unsigned int[]){DP_MSG_DISCONNECT_ALL_REASON},
                         DP_MSG_DISCONNECT_NUM_REASON),
        message, message_len);
    DP_free(message);
    return msg;
}

static DP_Message *generate_join(void)
{
    size_t name_len;
    char *name = generate_string(
        size_between(DP_MSG_JOIN_NAME_MIN_LEN, DP_MSG_JOIN_NAME_MAX_LEN),
        &name_len);
    size_t avatar_size = size_between(
        DP_MSG_JOIN_AVATAR_MIN_SIZE,
        DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_JOIN_STATIC_LENGTH - name_len);
    DP_Message *msg =
        DP_msg_join_new(generate_context_id(),
                        generate_flags((unsigned int[]){DP_MSG_JOIN_ALL_FLAGS},
                                       DP_MSG_JOIN_NUM_FLAGS),
                        name, name_len, generate_bytes, avatar_size, NULL);
    DP_free(name);
    return msg;
}

static DP_Message *generate_leave(void)
{
    return DP_msg_leave_new(generate_context_id());
}

static DP_Message *generate_session_owner(void)
{
    return DP_msg_session_owner_new(
        generate_context_id(), generate_uint8s,
        int_between(DP_MSG_SESSION_OWNER_USERS_MIN_COUNT,
                    DP_MSG_SESSION_OWNER_USERS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_trusted_users(void)
{
    return DP_msg_trusted_users_new(
        generate_context_id(), generate_uint8s,
        int_between(DP_MSG_TRUSTED_USERS_USERS_MIN_COUNT,
                    DP_MSG_TRUSTED_USERS_USERS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_soft_reset(void)
{
    return DP_msg_soft_reset_new(generate_context_id());
}

static DP_Message *generate_private_chat(void)
{
    size_t message_len;
    char *message =
        generate_string(size_between(DP_MSG_PRIVATE_CHAT_MESSAGE_MIN_LEN,
                                     DP_MSG_PRIVATE_CHAT_MESSAGE_MAX_LEN),
                        &message_len);
    DP_Message *msg =
        DP_msg_private_chat_new(generate_context_id(), random_uint8(),
                                random_uint8(), message, message_len);
    DP_free(message);
    return msg;
}

static DP_Message *generate_interval(void)
{
    return DP_msg_interval_new(generate_context_id(), random_uint16());
}

static DP_Message *generate_laser_trail(void)
{
    return DP_msg_laser_trail_new(generate_context_id(), random_uint32(),
                                  random_uint8());
}

static DP_Message *generate_move_pointer(void)
{
    return DP_msg_move_pointer_new(generate_context_id(), random_int32(),
                                   random_int32());
}

static DP_Message *generate_marker(void)
{
    size_t text_len;
    char *text = generate_string(
        size_between(DP_MSG_MARKER_TEXT_MIN_LEN, DP_MSG_MARKER_TEXT_MAX_LEN),
        &text_len);
    DP_Message *msg = DP_msg_marker_new(generate_context_id(), text, text_len);
    DP_free(text);
    return msg;
}

static DP_Message *generate_user_acl(void)
{
    return DP_msg_user_acl_new(generate_context_id(), generate_uint8s,
                               int_between(DP_MSG_USER_ACL_USERS_MIN_COUNT,
                                           DP_MSG_USER_ACL_USERS_MAX_COUNT),
                               NULL);
}

static DP_Message *generate_layer_acl(void)
{
    return DP_msg_layer_acl_new(
        generate_context_id(), random_uint16(), random_uint8(), generate_uint8s,
        int_between(DP_MSG_LAYER_ACL_EXCLUSIVE_MIN_COUNT,
                    DP_MSG_LAYER_ACL_EXCLUSIVE_MAX_COUNT),
        NULL);
}

static DP_Message *generate_feature_access_levels(void)
{
    return DP_msg_feature_access_levels_new(
        generate_context_id(), generate_uint8s,
        int_between(DP_MSG_FEATURE_ACCESS_LEVELS_FEATURE_TIERS_MIN_COUNT,
                    DP_MSG_FEATURE_ACCESS_LEVELS_FEATURE_TIERS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_default_layer(void)
{
    return DP_msg_default_layer_new(generate_context_id(), random_uint16());
}

static DP_Message *generate_filtered(void)
{
    return DP_msg_filtered_new(generate_context_id(), generate_bytes,
                               size_between(DP_MSG_FILTERED_MESSAGE_MIN_SIZE,
                                            DP_MSG_FILTERED_MESSAGE_MAX_SIZE),
                               NULL);
}

static DP_Message *generate_undo_depth(void)
{
    return DP_msg_undo_depth_new(generate_context_id(), random_uint8());
}

static DP_Message *generate_data(void)
{
    return DP_msg_data_new(
        generate_context_id(),
        generate_variant((unsigned int[]){DP_MSG_DATA_ALL_TYPE},
                         DP_MSG_DATA_NUM_TYPE),
        random_uint8(), generate_bytes,
        size_between(DP_MSG_DATA_BODY_MIN_SIZE, DP_MSG_DATA_BODY_MAX_SIZE),
        NULL);
}

static DP_Message *generate_local_change(void)
{
    return DP_msg_local_change_new(
        generate_context_id(),
        generate_variant((unsigned int[]){DP_MSG_LOCAL_CHANGE_ALL_TYPE},
                         DP_MSG_LOCAL_CHANGE_NUM_TYPE),
        generate_bytes,
        size_between(DP_MSG_LOCAL_CHANGE_BODY_MIN_SIZE,
                     DP_MSG_LOCAL_CHANGE_BODY_MAX_SIZE),
        NULL);
}

static DP_Message *generate_undo_point(void)
{
    return DP_msg_undo_point_new(generate_context_id());
}

static DP_Message *generate_canvas_resize(void)
{
    return DP_msg_canvas_resize_new(generate_context_id(), random_int32(),
                                    random_int32(), random_int32(),
                                    random_int32());
}

static DP_Message *generate_layer_create(void)
{
    size_t title_len;
    char *title =
        generate_string(size_between(DP_MSG_LAYER_CREATE_TITLE_MIN_LEN,
                                     DP_MSG_LAYER_CREATE_TITLE_MAX_LEN),
                        &title_len);
    DP_Message *msg = DP_msg_layer_create_new(
        generate_context_id(), random_uint16(), random_uint16(),
        random_uint32(),
        generate_flags((unsigned int[]){DP_MSG_LAYER_CREATE_ALL_FLAGS},
                       DP_MSG_LAYER_CREATE_NUM_FLAGS),
        title, title_len);
    DP_free(title);
    return msg;
}

static DP_Message *generate_layer_attributes(void)
{
    return DP_msg_layer_attributes_new(
        generate_context_id(), random_uint16(), random_uint8(),
        generate_flags((unsigned int[]){DP_MSG_LAYER_ATTRIBUTES_ALL_FLAGS},
                       DP_MSG_LAYER_ATTRIBUTES_NUM_FLAGS),
        random_uint8(), generate_blend_mode());
}

static DP_Message *generate_layer_retitle(void)
{
    size_t title_len;
    char *title =
        generate_string(size_between(DP_MSG_LAYER_RETITLE_TITLE_MIN_LEN,
                                     DP_MSG_LAYER_RETITLE_TITLE_MAX_LEN),
                        &title_len);
    DP_Message *msg = DP_msg_layer_retitle_new(
        generate_context_id(), random_uint16(), title, title_len);
    DP_free(title);
    return msg;
}

static DP_Message *generate_layer_order(void)
{
    return DP_msg_layer_order_new(
        generate_context_id(), generate_uint16s,
        int_between(DP_MSG_LAYER_ORDER_LAYERS_MIN_COUNT,
                    DP_MSG_LAYER_ORDER_LAYERS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_layer_delete(void)
{
    return DP_msg_layer_delete_new(generate_context_id(), random_uint16(),
                                   random_bool());
}

static DP_Message *generate_layer_visibility(void)
{
    return DP_msg_layer_visibility_new(generate_context_id(), random_uint16(),
                                       random_bool());
}

static DP_Message *generate_put_image(void)
{
    return DP_msg_put_image_new(generate_context_id(), random_uint16(),
                                generate_blend_mode(), random_uint32(),
                                random_uint32(), random_uint32(),
                                random_uint32(), generate_bytes,
                                size_between(DP_MSG_PUT_IMAGE_IMAGE_MIN_SIZE,
                                             DP_MSG_PUT_IMAGE_IMAGE_MAX_SIZE),
                                NULL);
}

static DP_Message *generate_fill_rect(void)
{
    return DP_msg_fill_rect_new(generate_context_id(), random_uint16(),
                                generate_blend_mode(), random_uint32(),
                                random_uint32(), random_uint32(),
                                random_uint32(), random_uint32());
}

static DP_Message *generate_pen_up(void)
{
    return DP_msg_pen_up_new(generate_context_id());
}

static DP_Message *generate_annotation_create(void)
{
    return DP_msg_annotation_create_new(generate_context_id(), random_uint16(),
                                        random_int32(), random_int32(),
                                        random_uint16(), random_uint16());
}

static DP_Message *generate_annotation_reshape(void)
{
    return DP_msg_annotation_reshape_new(generate_context_id(), random_uint16(),
                                         random_int32(), random_int32(),
                                         random_uint16(), random_uint16());
}

static DP_Message *generate_annotation_edit(void)
{
    size_t text_len;
    char *text =
        generate_string(size_between(DP_MSG_ANNOTATION_EDIT_TEXT_MIN_LEN,
                                     DP_MSG_ANNOTATION_EDIT_TEXT_MAX_LEN),
                        &text_len);
    DP_Message *msg = DP_msg_annotation_edit_new(
        generate_context_id(), random_uint16(), random_uint32(),
        generate_flags((unsigned int[]){DP_MSG_ANNOTATION_EDIT_ALL_FLAGS},
                       DP_MSG_ANNOTATION_EDIT_NUM_FLAGS),
        random_uint8(), text, text_len);
    DP_free(text);
    return msg;
}

static DP_Message *generate_annotation_delete(void)
{
    return DP_msg_annotation_delete_new(generate_context_id(), random_uint16());
}

static DP_Message *generate_move_region(void)
{
    return DP_msg_move_region_new(
        generate_context_id(), random_uint16(), random_int32(), random_int32(),
        random_int32(), random_int32(), random_int32(), random_int32(),
        random_int32(), random_int32(), random_int32(), random_int32(),
        random_int32(), random_int32(), generate_bytes,
        size_between(DP_MSG_MOVE_REGION_MASK_MIN_SIZE,
                     DP_MSG_MOVE_REGION_MASK_MAX_SIZE),
        NULL);
}

static DP_Message *generate_put_tile(void)
{
    return DP_msg_put_tile_new(generate_context_id(), random_uint16(),
                               random_uint8(), random_uint16(), random_uint16(),
                               random_uint16(), generate_bytes,
                               size_between(DP_MSG_PUT_TILE_IMAGE_MIN_SIZE,
                                            DP_MSG_PUT_TILE_IMAGE_MAX_SIZE),
                               NULL);
}

static DP_Message *generate_canvas_background(void)
{
    return DP_msg_canvas_background_new(
        generate_context_id(), generate_bytes,
        size_between(DP_MSG_CANVAS_BACKGROUND_IMAGE_MIN_SIZE,
                     DP_MSG_CANVAS_BACKGROUND_IMAGE_MAX_SIZE),
        NULL);
}

static DP_Message *generate_draw_dabs_classic(void)
{
    return DP_msg_draw_dabs_classic_new(
        generate_context_id(), random_uint16(), random_int32(), random_int32(),
        random_uint32(), generate_blend_mode(), generate_classic_dabs,
        int_between(DP_MSG_DRAW_DABS_CLASSIC_DABS_MIN_COUNT,
                    DP_MSG_DRAW_DABS_CLASSIC_DABS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_draw_dabs_pixel(void)
{
    return DP_msg_draw_dabs_pixel_new(
        generate_context_id(), random_uint16(), random_int32(), random_int32(),
        random_uint32(), generate_blend_mode(), generate_pixel_dabs,
        int_between(DP_MSG_DRAW_DABS_PIXEL_DABS_MIN_COUNT,
                    DP_MSG_DRAW_DABS_PIXEL_DABS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_draw_dabs_pixel_square(void)
{
    return DP_msg_draw_dabs_pixel_square_new(
        generate_context_id(), random_uint16(), random_int32(), random_int32(),
        random_uint32(), generate_blend_mode(), generate_pixel_dabs,
        int_between(DP_MSG_DRAW_DABS_PIXEL_DABS_MIN_COUNT,
                    DP_MSG_DRAW_DABS_PIXEL_DABS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_draw_dabs_mypaint(void)
{
    return DP_msg_draw_dabs_mypaint_new(
        generate_context_id(), random_uint16(), random_int32(), random_int32(),
        random_uint32(), random_uint8(), random_uint8(), random_uint8(),
        random_uint8(), generate_mypaint_dabs,
        int_between(DP_MSG_DRAW_DABS_MYPAINT_DABS_MIN_COUNT,
                    DP_MSG_DRAW_DABS_MYPAINT_DABS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_move_rect(void)
{
    return DP_msg_move_rect_new(generate_context_id(), random_uint16(),
                                random_uint16(), random_int32(), random_int32(),
                                random_int32(), random_int32(), random_int32(),
                                random_int32(), generate_bytes,
                                size_between(DP_MSG_MOVE_RECT_MASK_MIN_SIZE,
                                             DP_MSG_MOVE_REGION_MASK_MAX_SIZE),
                                NULL);
}

static DP_Message *generate_set_metadata_int(void)
{
    return DP_msg_set_metadata_int_new(
        generate_context_id(),
        generate_variant((unsigned int[]){DP_MSG_SET_METADATA_INT_ALL_FIELD},
                         DP_MSG_SET_METADATA_INT_NUM_FIELD),
        random_int32());
}

static DP_Message *generate_layer_tree_create(void)
{
    size_t title_len;
    char *title =
        generate_string(size_between(DP_MSG_LAYER_TREE_CREATE_TITLE_MIN_LEN,
                                     DP_MSG_LAYER_TREE_CREATE_TITLE_MAX_LEN),
                        &title_len);
    DP_Message *msg = DP_msg_layer_tree_create_new(
        generate_context_id(), random_uint16(), random_uint16(),
        random_uint16(), random_uint32(),
        generate_flags((unsigned int[]){DP_MSG_LAYER_TREE_CREATE_ALL_FLAGS},
                       DP_MSG_LAYER_TREE_CREATE_NUM_FLAGS),
        title, title_len);
    DP_free(title);
    return msg;
}

static DP_Message *generate_layer_tree_move(void)
{
    return DP_msg_layer_tree_move_new(generate_context_id(), random_uint16(),
                                      random_uint16(), random_uint16());
}

static DP_Message *generate_layer_tree_delete(void)
{
    return DP_msg_layer_tree_delete_new(generate_context_id(), random_uint16(),
                                        random_uint16());
}

static DP_Message *generate_transform_region(void)
{
    return DP_msg_transform_region_new(
        generate_context_id(), random_uint16(), random_uint16(), random_int32(),
        random_int32(), random_int32(), random_int32(), random_int32(),
        random_int32(), random_int32(), random_int32(), random_int32(),
        random_int32(), random_int32(), random_int32(),
        generate_variant((unsigned int[]){DP_MSG_TRANSFORM_REGION_ALL_MODE},
                         DP_MSG_TRANSFORM_REGION_NUM_MODE),
        generate_bytes,
        size_between(DP_MSG_TRANSFORM_REGION_MASK_MIN_SIZE,
                     DP_MSG_TRANSFORM_REGION_MASK_MAX_SIZE),
        NULL);
}

static DP_Message *generate_track_create(void)
{
    size_t title_len;
    char *title =
        generate_string(size_between(DP_MSG_TRACK_CREATE_TITLE_MIN_LEN,
                                     DP_MSG_TRACK_CREATE_TITLE_MAX_LEN),
                        &title_len);
    DP_Message *msg = DP_msg_track_create_new(
        generate_context_id(), random_uint16(), random_uint16(),
        random_uint16(), title, title_len);
    DP_free(title);
    return msg;
}

static DP_Message *generate_track_retitle(void)
{
    size_t title_len;
    char *title =
        generate_string(size_between(DP_MSG_TRACK_RETITLE_TITLE_MIN_LEN,
                                     DP_MSG_TRACK_RETITLE_TITLE_MAX_LEN),
                        &title_len);
    DP_Message *msg = DP_msg_track_retitle_new(
        generate_context_id(), random_uint16(), title, title_len);
    DP_free(title);
    return msg;
}

static DP_Message *generate_track_delete(void)
{
    return DP_msg_track_delete_new(generate_context_id(), random_uint16());
}

static DP_Message *generate_track_order(void)
{
    return DP_msg_track_order_new(
        generate_context_id(), generate_uint16s,
        int_between(DP_MSG_TRACK_ORDER_TRACKS_MIN_COUNT,
                    DP_MSG_TRACK_ORDER_TRACKS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_key_frame_set(void)
{
    return DP_msg_key_frame_set_new(
        generate_context_id(), random_uint16(), random_uint16(),
        random_uint16(), random_uint16(),
        generate_variant((unsigned int[]){DP_MSG_KEY_FRAME_SET_ALL_SOURCE},
                         DP_MSG_KEY_FRAME_SET_NUM_SOURCE));
}

static DP_Message *generate_key_frame_retitle(void)
{
    size_t title_len;
    char *title =
        generate_string(size_between(DP_MSG_KEY_FRAME_RETITLE_TITLE_MIN_LEN,
                                     DP_MSG_KEY_FRAME_RETITLE_TITLE_MAX_LEN),
                        &title_len);
    DP_Message *msg =
        DP_msg_key_frame_retitle_new(generate_context_id(), random_uint16(),
                                     random_uint16(), title, title_len);
    DP_free(title);
    return msg;
}

static DP_Message *generate_key_frame_layer_attributes(void)
{
    return DP_msg_key_frame_layer_attributes_new(
        generate_context_id(), random_uint16(), random_uint16(),
        generate_uint16s,
        int_between(DP_MSG_KEY_FRAME_LAYER_ATTRIBUTES_LAYERS_MIN_COUNT,
                    DP_MSG_KEY_FRAME_LAYER_ATTRIBUTES_LAYERS_MAX_COUNT),
        NULL);
}

static DP_Message *generate_key_frame_delete(void)
{
    return DP_msg_key_frame_delete_new(generate_context_id(), random_uint16(),
                                       random_uint16(), random_uint16(),
                                       random_uint16());
}

static DP_Message *generate_undo(void)
{
    return DP_msg_undo_new(generate_context_id(), random_uint8(),
                           random_bool());
}


typedef DP_Message *(*GenerateFn)(void);

static GenerateFn generate_fns[] = {
    generate_server_command,
    generate_disconnect,
    generate_ping,
    generate_join,
    generate_leave,
    generate_session_owner,
    generate_trusted_users,
    generate_soft_reset,
    generate_private_chat,
    generate_interval,
    generate_laser_trail,
    generate_move_pointer,
    generate_marker,
    generate_user_acl,
    generate_layer_acl,
    generate_feature_access_levels,
    generate_default_layer,
    generate_filtered,
    generate_undo_depth,
    generate_data,
    generate_local_change,
    generate_undo_point,
    generate_canvas_resize,
    generate_layer_create,
    generate_layer_attributes,
    generate_layer_retitle,
    generate_layer_order,
    generate_layer_delete,
    generate_layer_visibility,
    generate_put_image,
    generate_fill_rect,
    generate_pen_up,
    generate_annotation_create,
    generate_annotation_reshape,
    generate_annotation_edit,
    generate_annotation_delete,
    generate_move_region,
    generate_put_tile,
    generate_canvas_background,
    generate_draw_dabs_classic,
    generate_draw_dabs_pixel,
    generate_draw_dabs_pixel_square,
    generate_draw_dabs_mypaint,
    generate_move_rect,
    generate_set_metadata_int,
    generate_layer_tree_create,
    generate_layer_tree_move,
    generate_layer_tree_delete,
    generate_transform_region,
    generate_track_create,
    generate_track_retitle,
    generate_track_delete,
    generate_track_order,
    generate_key_frame_set,
    generate_key_frame_retitle,
    generate_key_frame_layer_attributes,
    generate_key_frame_delete,
    generate_undo,
};

static void write_messages(TEST_PARAMS, DP_BinaryWriter *bw, DP_TextWriter *tw)
{
    int count = DP_ARRAY_LENGTH(generate_fns);
    for (int i = 0; i < count; ++i) {
        DP_Message *msg = generate_fns[i]();
        write_message_binary(TEST_ARGS, msg, bw);
        write_message_text(TEST_ARGS, msg, tw);
        DP_message_decref(msg);
    }
}

static void write_initial_messages(TEST_PARAMS)
{
    DP_BinaryWriter *bw =
        open_binary_writer(TEST_ARGS, "test/tmp/roundtrip_base.dprec");
    DP_TextWriter *tw =
        open_text_writer(TEST_ARGS, "test/tmp/roundtrip_base.dptxt");
    write_initial_header(TEST_ARGS, bw, tw);
    write_messages(TEST_ARGS, bw, tw);
    DP_binary_writer_free(bw);
    DP_text_writer_free(tw);
}

static void read_binary_messages(TEST_PARAMS)
{
    DP_BinaryReader *br =
        open_binary_reader(TEST_ARGS, "test/tmp/roundtrip_base.dprec");
    DP_BinaryWriter *bw =
        open_binary_writer(TEST_ARGS, "test/tmp/roundtrip_from_dprec.dprec");
    DP_TextWriter *tw =
        open_text_writer(TEST_ARGS, "test/tmp/roundtrip_from_dprec.dptxt");

    write_header(TEST_ARGS, bw, tw,
                 json_value_get_object(DP_binary_reader_header(br)));

    while (true) {
        DP_Message *msg;
        DP_BinaryReaderResult result =
            DP_binary_reader_read_message(br, true, &msg);
        OK(result == DP_BINARY_READER_SUCCESS
               || result == DP_BINARY_READER_INPUT_END,
           "read binary message");
        if (result == DP_BINARY_READER_SUCCESS) {
            write_message_binary(TEST_ARGS, msg, bw);
            write_message_text(TEST_ARGS, msg, tw);
            DP_message_decref(msg);
        }
        else if (result == DP_BINARY_READER_INPUT_END) {
            break;
        }
        else if (result == DP_BINARY_READER_ERROR_INPUT) {
            DIAG("Binary reader input error %s", DP_error());
            break;
        }
        else if (result == DP_BINARY_READER_ERROR_PARSE) {
            DIAG("Binary reader parse error %s", DP_error());
            break;
        }
        else {
            DIAG("Binary reader unknown error %s", DP_error());
            break;
        }
    }

    DP_binary_writer_free(bw);
    DP_text_writer_free(tw);
    DP_binary_reader_free(br);
}

static void read_text_messages(TEST_PARAMS)
{
    DP_TextReader *tr =
        open_text_reader(TEST_ARGS, "test/tmp/roundtrip_base.dptxt");
    DP_BinaryWriter *bw =
        open_binary_writer(TEST_ARGS, "test/tmp/roundtrip_from_dptxt.dprec");
    DP_TextWriter *tw =
        open_text_writer(TEST_ARGS, "test/tmp/roundtrip_from_dptxt.dptxt");

    bool healthy = true;
    {
        JSON_Value *header_value = json_value_init_object();
        JSON_Object *header = json_value_get_object(header_value);
        while (healthy) {
            const char *key, *value;
            DP_TextReaderResult result =
                DP_text_reader_read_header_field(tr, &key, &value);
            healthy = OK(result == DP_TEXT_READER_SUCCESS
                             || result == DP_TEXT_READER_HEADER_END,
                         "read text header field");
            if (result == DP_TEXT_READER_SUCCESS) {
                json_object_set_string(header, key, value);
            }
            else if (result == DP_TEXT_READER_HEADER_END) {
                break;
            }
            else if (result == DP_TEXT_READER_ERROR_INPUT) {
                DIAG("Text reader header input error %s", DP_error());
            }
            else if (result == DP_TEXT_READER_ERROR_PARSE) {
                DIAG("Text reader header parse error %s", DP_error());
            }
            else {
                DIAG("Text reader header unknown error %s", DP_error());
            }
        }
        write_header(TEST_ARGS, bw, tw, header);
        json_value_free(header_value);
    }

    while (healthy) {
        DP_Message *msg;
        DP_TextReaderResult result = DP_text_reader_read_message(tr, &msg);
        healthy = OK(result == DP_TEXT_READER_SUCCESS
                         || result == DP_TEXT_READER_INPUT_END,
                     "read text message");
        if (result == DP_TEXT_READER_SUCCESS) {
            write_message_binary(TEST_ARGS, msg, bw);
            write_message_text(TEST_ARGS, msg, tw);
            DP_message_decref(msg);
        }
        else if (result == DP_TEXT_READER_INPUT_END) {
            break;
        }
        else if (result == DP_TEXT_READER_ERROR_INPUT) {
            DIAG("Text reader input error %s", DP_error());
        }
        else if (result == DP_TEXT_READER_ERROR_PARSE) {
            DIAG("Text reader parse error %s", DP_error());
        }
        else {
            DIAG("Text reader unknown error %s", DP_error());
        }
    }

    DP_binary_writer_free(bw);
    DP_text_writer_free(tw);
    DP_text_reader_free(tr);
}

static void read_write_roundtrip(TEST_PARAMS)
{
    write_initial_messages(TEST_ARGS);
    read_binary_messages(TEST_ARGS);
    read_text_messages(TEST_ARGS);
    FILE_EQ_OK("test/tmp/roundtrip_from_dprec.dprec",
               "test/tmp/roundtrip_base.dprec",
               "roundtrip from dprec to dprec");
    FILE_EQ_OK("test/tmp/roundtrip_from_dprec.dptxt",
               "test/tmp/roundtrip_base.dptxt",
               "roundtrip from dprec to dptxt");
    FILE_EQ_OK("test/tmp/roundtrip_from_dptxt.dprec",
               "test/tmp/roundtrip_base.dprec",
               "roundtrip from dptxt to dprec");
    FILE_EQ_OK("test/tmp/roundtrip_from_dptxt.dptxt",
               "test/tmp/roundtrip_base.dptxt",
               "roundtrip from dptxt to dptxt");
}


static bool push_consumer_message(void *user, DP_Message *msg)
{
    DP_Vector *out_msgs = user;
    DP_message_vector_push_noinc(out_msgs, msg);
    return true;
}

static void free_stream_messages(int count, DP_Message **msgs)
{
    for (int i = 0; i < count; ++i) {
        DP_message_decref(msgs[i]);
    }
    DP_free(msgs);
}

static void reset_stream_roundtrip(TEST_PARAMS)
{
    size_t count = (size_t)1 + (size_t)random_uint32() % (size_t)1000;
    NOTE("Testing round-trip with %zu message(s)", count);

    DP_Vector in_msgs;
    DP_message_vector_init(&in_msgs, count);
    for (size_t i = 0; i < count; ++i) {
        DP_Message *msg = generate_fns[DP_uint32_to_size(random_uint32())
                                       % DP_ARRAY_LENGTH(generate_fns)]();
        DP_message_vector_push_noinc(&in_msgs, msg);
    }

    DP_ResetStreamProducer *rsp = DP_reset_stream_producer_new();
    if (!NOT_NULL_OK(rsp, "producer created")) {
        DP_message_vector_dispose(&in_msgs);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!OK(DP_reset_stream_producer_push(
                    rsp, DP_message_vector_at(&in_msgs, i)),
                "push producer message %zu", i)) {
            DP_reset_stream_producer_free_discard(rsp);
            DP_message_vector_dispose(&in_msgs);
            return;
        }
    }

    int stream_count;
    DP_Message **stream_msgs =
        DP_reset_stream_producer_free_finish(rsp, &stream_count);
    if (!OK(stream_msgs, "producer finished")
        || !OK(stream_count > 0, "stream message count %d", stream_count)) {
        DP_message_vector_dispose(&in_msgs);
        return;
    }

    DP_Vector out_msgs;
    DP_message_vector_init(&out_msgs, count);

    DP_ResetStreamConsumer *rsc =
        DP_reset_stream_consumer_new(push_consumer_message, &out_msgs, true);
    if (!NOT_NULL_OK(rsc, "consumer created")) {
        DP_message_vector_dispose(&out_msgs);
        free_stream_messages(stream_count, stream_msgs);
        DP_message_vector_dispose(&in_msgs);
        return;
    }

    for (int i = 0; i < stream_count; ++i) {
        DP_Message *msg = stream_msgs[i];
        if (INT_EQ_OK(DP_message_type(msg), DP_MSG_RESET_STREAM,
                      "stream message %d is reset stream", i)) {
            size_t size;
            const unsigned char *data =
                DP_msg_reset_stream_data(DP_message_internal(msg), &size);
            if (OK(data && size != 0, "stream message %d has data", i)) {
                if (!OK(DP_reset_stream_consumer_push(rsc, data, size),
                        "push consumer message %d", i)) {
                    DP_reset_stream_consumer_free_discard(rsc);
                    DP_message_vector_dispose(&out_msgs);
                    free_stream_messages(stream_count, stream_msgs);
                    DP_message_vector_dispose(&in_msgs);
                    return;
                }
            }
        }
    }

    free_stream_messages(stream_count, stream_msgs);
    if (OK(DP_reset_stream_consumer_free_finish(rsc), "free consumer")) {
        if (UINT_EQ_OK(out_msgs.used, in_msgs.used, "message counts match")) {
            for (size_t i = 0; i < in_msgs.used; ++i) {
                DP_Message *in_msg = DP_message_vector_at(&in_msgs, i);
                DP_Message *out_msg = DP_message_vector_at(&in_msgs, i);
                OK(DP_message_equals(out_msg, in_msg), "message %zu is equal",
                   i);
            }
        }
    }

    DP_message_vector_dispose(&out_msgs);
    DP_message_vector_dispose(&in_msgs);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(read_write_roundtrip);
    REGISTER_TEST(reset_stream_roundtrip);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
