/*
 * Copyright (C) 2022 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#include "draw_dabs.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <math.h>


#define MIN_PAYLOAD_LENGTH 15
#define CLASSIC_DAB_LENGTH 6
#define PIXEL_DAB_LENGTH   4
#define MAX_CLASSIC_DAB_COUNT \
    ((UINT16_MAX - MIN_PAYLOAD_LENGTH) / CLASSIC_DAB_LENGTH)
#define MAX_PIXEL_DAB_COUNT \
    ((UINT16_MAX - MIN_PAYLOAD_LENGTH) / PIXEL_DAB_LENGTH)

struct DP_BrushDab {
    int8_t x;
    int8_t y;
    uint8_t opacity;
};

struct DP_ClassicBrushDab {
    DP_BrushDab base;
    uint16_t size;
    uint8_t hardness;
};

struct DP_PixelBrushDab {
    DP_BrushDab base;
    uint8_t size;
};

struct DP_MsgDrawDabs {
    uint16_t layer_id;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t color;
    uint8_t blend_mode;
    int dab_count;
};

struct DP_MsgDrawDabsClassic {
    DP_MsgDrawDabs base;
    DP_ClassicBrushDab dabs[];
};

struct DP_MsgDrawDabsPixel {
    DP_MsgDrawDabs base;
    DP_PixelBrushDab dabs[];
};


int DP_brush_dab_x(DP_BrushDab *dab)
{
    DP_ASSERT(dab);
    return dab->x;
}

int DP_brush_dab_y(DP_BrushDab *dab)
{
    DP_ASSERT(dab);
    return dab->y;
}

uint8_t DP_brush_dab_opacity(DP_BrushDab *dab)
{
    return dab->opacity;
}

static bool dab_equals(DP_BrushDab *DP_RESTRICT a, DP_BrushDab *DP_RESTRICT b)
{
    DP_ASSERT(a);
    DP_ASSERT(b);
    return a->x == b->x && a->y == b->y && a->opacity == b->opacity;
}


DP_BrushDab *DP_classic_brush_dab_base(DP_ClassicBrushDab *dab)
{
    DP_ASSERT(dab);
    return &dab->base;
}

int DP_classic_brush_dab_x(DP_ClassicBrushDab *dab)
{
    DP_ASSERT(dab);
    return DP_brush_dab_x(&dab->base);
}

int DP_classic_brush_dab_y(DP_ClassicBrushDab *dab)
{
    DP_ASSERT(dab);
    return DP_brush_dab_y(&dab->base);
}

int DP_classic_brush_dab_size(DP_ClassicBrushDab *dab)
{
    DP_ASSERT(dab);
    return dab->size;
}

uint8_t DP_classic_brush_dab_hardness(DP_ClassicBrushDab *dab)
{
    DP_ASSERT(dab);
    return dab->hardness;
}

uint8_t DP_classic_brush_dab_opacity(DP_ClassicBrushDab *dab)
{
    DP_ASSERT(dab);
    return DP_brush_dab_opacity(&dab->base);
}

DP_ClassicBrushDab *DP_classic_brush_dab_at(DP_ClassicBrushDab *dabs, int i)
{
    DP_ASSERT(dabs);
    return &dabs[i];
}


DP_BrushDab *DP_pixel_brush_dab_base(DP_PixelBrushDab *dab)
{
    DP_ASSERT(dab);
    return &dab->base;
}

int DP_pixel_brush_dab_x(DP_PixelBrushDab *dab)
{
    DP_ASSERT(dab);
    return DP_brush_dab_x(&dab->base);
}

int DP_pixel_brush_dab_y(DP_PixelBrushDab *dab)
{
    DP_ASSERT(dab);
    return DP_brush_dab_y(&dab->base);
}

int DP_pixel_brush_dab_size(DP_PixelBrushDab *dab)
{
    DP_ASSERT(dab);
    return dab->size;
}

uint8_t DP_pixel_brush_dab_opacity(DP_PixelBrushDab *dab)
{
    DP_ASSERT(dab);
    return DP_brush_dab_opacity(&dab->base);
}

DP_PixelBrushDab *DP_pixel_brush_dab_at(DP_PixelBrushDab *dabs, int i)
{
    DP_ASSERT(dabs);
    return &dabs[i];
}


static size_t base_serialize(DP_MsgDrawDabs *mdd, unsigned char *data)
{
    DP_ASSERT(mdd);
    DP_ASSERT(data);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mdd->layer_id, data + written);
    written += DP_write_bigendian_int32(mdd->origin_x, data + written);
    written += DP_write_bigendian_int32(mdd->origin_y, data + written);
    written += DP_write_bigendian_uint32(mdd->color, data + written);
    written += DP_write_bigendian_uint8(mdd->blend_mode, data + written);
    return written;
}

static bool base_equals(DP_MsgDrawDabs *DP_RESTRICT a,
                        DP_MsgDrawDabs *DP_RESTRICT b)
{
    DP_ASSERT(a);
    DP_ASSERT(b);
    return a->layer_id == b->layer_id && a->origin_x == b->origin_x
        && a->origin_y == b->origin_y && a->blend_mode == b->blend_mode
        && a->dab_count == b->dab_count;
}

static void base_init(DP_MsgDrawDabs *mdd, int layer_id, int origin_x,
                      int origin_y, uint32_t color, int blend_mode,
                      int dab_count)
{
    DP_ASSERT(mdd);
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(origin_x >= INT32_MIN);
    DP_ASSERT(origin_x <= INT32_MAX);
    DP_ASSERT(origin_y >= INT32_MIN);
    DP_ASSERT(origin_y <= INT32_MAX);
    DP_ASSERT(blend_mode >= 0);
    DP_ASSERT(blend_mode <= UINT8_MAX);
    mdd->layer_id = DP_int_to_uint16(layer_id);
    mdd->origin_x = DP_int_to_int32(origin_x);
    mdd->origin_y = DP_int_to_int32(origin_y);
    mdd->color = color;
    mdd->blend_mode = DP_int_to_uint8(blend_mode);
    mdd->dab_count = dab_count;
}


DP_MsgDrawDabs *DP_msg_draw_dabs_cast(DP_Message *msg)
{
    return DP_message_cast3(msg, DP_MSG_DRAW_DABS_CLASSIC,
                            DP_MSG_DRAW_DABS_PIXEL,
                            DP_MSG_DRAW_DABS_PIXEL_SQUARE);
}

DP_MsgDrawDabsClassic *DP_msg_draw_dabs_cast_classic(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(!mdd
              || DP_message_type(DP_message_from_internal(mdd))
                     == DP_MSG_DRAW_DABS_CLASSIC);
    return (DP_MsgDrawDabsClassic *)mdd;
}

DP_MsgDrawDabsPixel *DP_msg_draw_dabs_cast_pixel(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(!mdd
              || DP_message_type(DP_message_from_internal(mdd))
                     == DP_MSG_DRAW_DABS_PIXEL
              || DP_message_type(DP_message_from_internal(mdd))
                     == DP_MSG_DRAW_DABS_PIXEL_SQUARE);
    return (DP_MsgDrawDabsPixel *)mdd;
}

int DP_msg_draw_dabs_layer_id(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(mdd);
    return mdd->layer_id;
}

int DP_msg_draw_dabs_origin_x(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(mdd);
    return mdd->origin_x;
}

int DP_msg_draw_dabs_origin_y(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(mdd);
    return mdd->origin_y;
}

uint32_t DP_msg_draw_dabs_color(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(mdd);
    return mdd->color;
}

int DP_msg_draw_dabs_blend_mode(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(mdd);
    return mdd->blend_mode;
}

bool DP_msg_draw_dabs_indirect(DP_MsgDrawDabs *mdd)
{
    DP_ASSERT(mdd);
    return mdd->color & 0xff000000;
}

static bool dabs_check_count(size_t dab_length, size_t length,
                             int *out_dab_count)
{
    size_t dabs_length = length - MIN_PAYLOAD_LENGTH;
    if (dabs_length % dab_length == 0) {
        *out_dab_count = DP_size_to_int(dabs_length / dab_length);
        return true;
    }
    else {
        return false;
    }
}


static size_t classic_payload_length(DP_Message *msg)
{
    DP_MsgDrawDabsClassic *mddc = DP_msg_draw_dabs_classic_cast(msg);
    DP_ASSERT(mddc);
    return MIN_PAYLOAD_LENGTH
         + DP_int_to_size(mddc->base.dab_count) * CLASSIC_DAB_LENGTH;
}

static size_t classic_serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgDrawDabsClassic *mddc = DP_msg_draw_dabs_classic_cast(msg);
    DP_ASSERT(mddc);

    size_t written = base_serialize(&mddc->base, data);

    int dab_count = mddc->base.dab_count;
    for (int i = 0; i < dab_count; ++i) {
        DP_ClassicBrushDab *dab = &mddc->dabs[i];
        written += DP_write_bigendian_int8(dab->base.x, data + written);
        written += DP_write_bigendian_int8(dab->base.y, data + written);
        written += DP_write_bigendian_uint16(dab->size, data + written);
        written += DP_write_bigendian_uint8(dab->hardness, data + written);
        written += DP_write_bigendian_uint8(dab->base.opacity, data + written);
    }

    return written;
}

static double round_subpixel(int value)
{
    return round(value / 4.0 * 10.0) / 10.0;
}

static bool classic_write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgDrawDabsClassic *mddc = DP_msg_draw_dabs_classic_cast(msg);
    DP_ASSERT(mddc);
    DP_MsgDrawDabs *mdd = &mddc->base;

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mdd->layer_id));

    DP_RETURN_UNLESS(DP_text_writer_raw_format(writer, " x=%.1f y=%.1f",
                                               round_subpixel(mdd->origin_x),
                                               round_subpixel(mdd->origin_y)));

    DP_RETURN_UNLESS(
        DP_text_writer_write_argb_color(writer, "color", mdd->color));

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "mode", mdd->blend_mode));

    DP_RETURN_UNLESS(DP_text_writer_raw_write(writer, " {\n\t", 4));

    int dab_count = mdd->dab_count;
    for (int i = 0; i < dab_count; ++i) {
        DP_ClassicBrushDab *dab = &mddc->dabs[i];
        DP_RETURN_UNLESS(DP_text_writer_raw_format(
            writer, "%.1f %.1f %u %d %d\n\t", round_subpixel(dab->base.x),
            round_subpixel(dab->base.y), DP_uint16_to_uint(dab->size),
            DP_uint8_to_int(dab->hardness),
            DP_uint8_to_int(dab->base.opacity)));
    }

    DP_RETURN_UNLESS(DP_text_writer_raw_write(writer, "}", 1));
    return true;
}

static bool classic_dab_equals(DP_ClassicBrushDab *DP_RESTRICT a,
                               DP_ClassicBrushDab *DP_RESTRICT b)
{
    return dab_equals(&a->base, &b->base) && a->size == b->size
        && a->hardness == b->hardness;
}

static bool classic_dabs_equal(int dab_count, DP_ClassicBrushDab *DP_RESTRICT a,
                               DP_ClassicBrushDab *DP_RESTRICT b)
{
    for (int i = 0; i < dab_count; ++i) {
        if (!classic_dab_equals(&a[i], &b[i])) {
            return false;
        }
    }
    return true;
}

static bool classic_equals(DP_Message *DP_RESTRICT msg,
                           DP_Message *DP_RESTRICT other)
{
    DP_MsgDrawDabsClassic *a = DP_msg_draw_dabs_classic_cast(msg);
    DP_MsgDrawDabsClassic *b = DP_msg_draw_dabs_classic_cast(other);
    DP_ASSERT(a);
    DP_ASSERT(b);
    return base_equals(&a->base, &b->base)
        && classic_dabs_equal(a->base.dab_count, a->dabs, b->dabs);
}

static const DP_MessageMethods classic_methods = {
    classic_payload_length,
    classic_serialize_payload,
    classic_write_payload_text,
    classic_equals,
    NULL,
};

DP_Message *DP_msg_draw_dabs_classic_new(unsigned int context_id, int layer_id,
                                         int origin_x, int origin_y,
                                         uint32_t color, int blend_mode,
                                         int dab_count)
{
    DP_ASSERT(dab_count > 0);
    DP_ASSERT(dab_count <= MAX_CLASSIC_DAB_COUNT);
    DP_MsgDrawDabsClassic *mddc;
    size_t dabs_size = DP_int_to_size(dab_count) * sizeof(DP_ClassicBrushDab);
    DP_Message *msg =
        DP_message_new(DP_MSG_DRAW_DABS_CLASSIC, context_id, &classic_methods,
                       sizeof(*mddc) + dabs_size);
    mddc = DP_message_internal(msg);
    base_init(&mddc->base, layer_id, origin_x, origin_y, color, blend_mode,
              dab_count);
    return msg;
}

static void classic_dabs_deserialize(DP_Message *msg,
                                     const unsigned char *dab_buffer)
{
    DP_MsgDrawDabsClassic *mddc = DP_message_internal(msg);
    int dab_count = mddc->base.dab_count;
    for (int i = 0; i < dab_count; ++i) {
        const unsigned char *d = dab_buffer + i * CLASSIC_DAB_LENGTH;
        mddc->dabs[i] = (DP_ClassicBrushDab){{DP_read_bigendian_int8(d),
                                              DP_read_bigendian_int8(d + 1),
                                              DP_read_bigendian_uint8(d + 5)},
                                             DP_read_bigendian_uint16(d + 2),
                                             DP_read_bigendian_uint8(d + 4)};
    }
}

DP_Message *DP_msg_draw_dabs_classic_deserialize(unsigned int context_id,
                                                 const unsigned char *buffer,
                                                 size_t length)
{
    int dab_count;
    if (length >= MIN_PAYLOAD_LENGTH
        && dabs_check_count(CLASSIC_DAB_LENGTH, length, &dab_count)) {
        DP_Message *msg = DP_msg_draw_dabs_classic_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_int32(buffer + 2),
            DP_read_bigendian_int32(buffer + 6),
            DP_read_bigendian_uint32(buffer + 10),
            DP_read_bigendian_uint8(buffer + 14), dab_count);
        classic_dabs_deserialize(msg, buffer + MIN_PAYLOAD_LENGTH);
        return msg;
    }
    else {
        DP_error_set("Wrong length for DRAW_DABS_CLASSIC message: %zu", length);
        return NULL;
    }
}


DP_MsgDrawDabsClassic *DP_msg_draw_dabs_classic_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_DRAW_DABS_CLASSIC);
}


DP_MsgDrawDabs *DP_msg_draw_dabs_classic_base(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return &mddc->base;
}

int DP_msg_draw_dabs_classic_layer_id(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return DP_msg_draw_dabs_layer_id(&mddc->base);
}

int DP_msg_draw_dabs_classic_origin_x(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return DP_msg_draw_dabs_origin_x(&mddc->base);
}

int DP_msg_draw_dabs_classic_origin_y(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return DP_msg_draw_dabs_origin_y(&mddc->base);
}

uint32_t DP_msg_draw_dabs_classic_color(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return DP_msg_draw_dabs_color(&mddc->base);
}

int DP_msg_draw_dabs_classic_blend_mode(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return DP_msg_draw_dabs_blend_mode(&mddc->base);
}

DP_ClassicBrushDab *DP_msg_draw_dabs_classic_dabs(DP_MsgDrawDabsClassic *mddc,
                                                  int *out_count)
{
    DP_ASSERT(mddc);
    if (out_count) {
        *out_count = mddc->base.dab_count;
    }
    return mddc->dabs;
}

bool DP_msg_draw_dabs_classic_indirect(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return DP_msg_draw_dabs_indirect(&mddc->base);
}


static size_t pixel_payload_length(DP_Message *msg)
{
    DP_MsgDrawDabsPixel *mddp = DP_msg_draw_dabs_pixel_cast(msg);
    DP_ASSERT(mddp);
    return MIN_PAYLOAD_LENGTH
         + DP_int_to_size(mddp->base.dab_count) * PIXEL_DAB_LENGTH;
}

static size_t pixel_serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgDrawDabsPixel *mddp = DP_msg_draw_dabs_pixel_cast(msg);
    DP_ASSERT(mddp);

    size_t written = base_serialize(&mddp->base, data);

    int dab_count = mddp->base.dab_count;
    for (int i = 0; i < dab_count; ++i) {
        DP_PixelBrushDab *dab = &mddp->dabs[i];
        written += DP_write_bigendian_int8(dab->base.x, data + written);
        written += DP_write_bigendian_int8(dab->base.y, data + written);
        written += DP_write_bigendian_uint8(dab->size, data + written);
        written += DP_write_bigendian_uint8(dab->base.opacity, data + written);
    }

    return written;
}

static bool pixel_write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgDrawDabsPixel *mddp = DP_msg_draw_dabs_pixel_cast(msg);
    DP_ASSERT(mddp);
    DP_MsgDrawDabs *mdd = &mddp->base;

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mdd->layer_id));

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x", mdd->origin_x));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y", mdd->origin_y));

    DP_RETURN_UNLESS(
        DP_text_writer_write_argb_color(writer, "color", mdd->color));

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "mode", mdd->blend_mode));

    DP_RETURN_UNLESS(DP_text_writer_raw_write(writer, " {\n\t", 4));

    int dab_count = mdd->dab_count;
    for (int i = 0; i < dab_count; ++i) {
        DP_PixelBrushDab *dab = &mddp->dabs[i];
        DP_RETURN_UNLESS(DP_text_writer_raw_format(
            writer, "%d %d %u %d\n\t", DP_int8_to_int(dab->base.x),
            DP_int8_to_int(dab->base.y), DP_uint8_to_uint(dab->size),
            DP_uint8_to_int(dab->base.opacity)));
    }

    DP_RETURN_UNLESS(DP_text_writer_raw_write(writer, "}", 1));
    return true;
}

static bool pixel_dab_equals(DP_PixelBrushDab *DP_RESTRICT a,
                             DP_PixelBrushDab *DP_RESTRICT b)
{
    return dab_equals(&a->base, &b->base) && a->size == b->size;
}

static bool pixel_dabs_equal(int dab_count, DP_PixelBrushDab *DP_RESTRICT a,
                             DP_PixelBrushDab *DP_RESTRICT b)
{
    for (int i = 0; i < dab_count; ++i) {
        if (!pixel_dab_equals(&a[i], &b[i])) {
            return false;
        }
    }
    return true;
}

static bool pixel_equals(DP_Message *DP_RESTRICT msg,
                         DP_Message *DP_RESTRICT other)
{
    DP_MsgDrawDabsPixel *a = DP_msg_draw_dabs_pixel_cast(msg);
    DP_MsgDrawDabsPixel *b = DP_msg_draw_dabs_pixel_cast(other);
    DP_ASSERT(a);
    DP_ASSERT(b);
    return base_equals(&a->base, &b->base)
        && pixel_dabs_equal(a->base.dab_count, a->dabs, b->dabs);
}

static const DP_MessageMethods pixel_methods = {
    pixel_payload_length,
    pixel_serialize_payload,
    pixel_write_payload_text,
    pixel_equals,
    NULL,
};

DP_Message *DP_msg_draw_dabs_pixel_new(int type, unsigned int context_id,
                                       int layer_id, int origin_x, int origin_y,
                                       uint32_t color, int blend_mode,
                                       int dab_count)
{
    DP_ASSERT(type == DP_MSG_DRAW_DABS_PIXEL
              || type == DP_MSG_DRAW_DABS_PIXEL_SQUARE);
    DP_ASSERT(dab_count > 0);
    DP_ASSERT(dab_count <= MAX_PIXEL_DAB_COUNT);
    DP_MsgDrawDabsPixel *mddp;
    size_t dabs_size = DP_int_to_size(dab_count) * sizeof(DP_PixelBrushDab);
    DP_Message *msg = DP_message_new((DP_MessageType)type, context_id,
                                     &pixel_methods, sizeof(*mddp) + dabs_size);
    mddp = DP_message_internal(msg);
    base_init(&mddp->base, layer_id, origin_x, origin_y, color, blend_mode,
              dab_count);
    return msg;
}

static void pixel_dabs_deserialize(DP_Message *msg,
                                   const unsigned char *dab_buffer)
{
    DP_MsgDrawDabsPixel *mddp = DP_message_internal(msg);
    int dab_count = mddp->base.dab_count;
    for (int i = 0; i < dab_count; ++i) {
        const unsigned char *d = dab_buffer + i * PIXEL_DAB_LENGTH;
        mddp->dabs[i] = (DP_PixelBrushDab){{DP_read_bigendian_int8(d),
                                            DP_read_bigendian_int8(d + 1),
                                            DP_read_bigendian_uint8(d + 3)},
                                           DP_read_bigendian_uint8(d + 2)};
    }
}

static DP_Message *pixel_deserialize(int type, unsigned int context_id,
                                     const unsigned char *buffer, size_t length)
{
    int dab_count;
    if (length >= MIN_PAYLOAD_LENGTH
        && dabs_check_count(PIXEL_DAB_LENGTH, length, &dab_count)) {
        DP_Message *msg = DP_msg_draw_dabs_pixel_new(
            type, context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_int32(buffer + 2),
            DP_read_bigendian_int32(buffer + 6),
            DP_read_bigendian_uint32(buffer + 10),
            DP_read_bigendian_uint8(buffer + 14), dab_count);
        pixel_dabs_deserialize(msg, buffer + MIN_PAYLOAD_LENGTH);
        return msg;
    }
    else {
        DP_error_set("Wrong length for %s message: %zu",
                     DP_message_type_enum_name_unprefixed((DP_MessageType)type),
                     length);
        return NULL;
    }
}

DP_Message *DP_msg_draw_dabs_pixel_deserialize(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length)
{
    return pixel_deserialize(DP_MSG_DRAW_DABS_PIXEL, context_id, buffer,
                             length);
}

DP_Message *DP_msg_draw_dabs_pixel_square_deserialize(
    unsigned int context_id, const unsigned char *buffer, size_t length)
{
    return pixel_deserialize(DP_MSG_DRAW_DABS_PIXEL_SQUARE, context_id, buffer,
                             length);
}


DP_MsgDrawDabsPixel *DP_msg_draw_dabs_pixel_cast(DP_Message *msg)
{
    return DP_message_cast2(msg, DP_MSG_DRAW_DABS_PIXEL,
                            DP_MSG_DRAW_DABS_PIXEL_SQUARE);
}


DP_MsgDrawDabs *DP_msg_draw_dabs_pixel_base(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return &mddp->base;
}

int DP_msg_draw_dabs_pixel_layer_id(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return DP_msg_draw_dabs_layer_id(&mddp->base);
}

int DP_msg_draw_dabs_pixel_origin_x(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return DP_msg_draw_dabs_origin_x(&mddp->base);
}

int DP_msg_draw_dabs_pixel_origin_y(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return DP_msg_draw_dabs_origin_y(&mddp->base);
}

uint32_t DP_msg_draw_dabs_pixel_color(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return DP_msg_draw_dabs_color(&mddp->base);
}

int DP_msg_draw_dabs_pixel_blend_mode(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return DP_msg_draw_dabs_blend_mode(&mddp->base);
}

DP_PixelBrushDab *DP_msg_draw_dabs_pixel_dabs(DP_MsgDrawDabsPixel *mddp,
                                              int *out_count)
{
    DP_ASSERT(mddp);
    if (out_count) {
        *out_count = mddp->base.dab_count;
    }
    return mddp->dabs;
}

bool DP_msg_draw_dabs_pixel_indirect(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return DP_msg_draw_dabs_indirect(&mddp->base);
}
