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
#ifndef DPMSG_DRAW_DABS_CLASSIC_H
#define DPMSG_DRAW_DABS_CLASSIC_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_BrushDab DP_BrushDab;
typedef struct DP_ClassicBrushDab DP_ClassicBrushDab;
typedef struct DP_PixelBrushDab DP_PixelBrushDab;

typedef struct DP_MsgDrawDabs DP_MsgDrawDabs;
typedef struct DP_MsgDrawDabsClassic DP_MsgDrawDabsClassic;
typedef struct DP_MsgDrawDabsPixel DP_MsgDrawDabsPixel;


int DP_brush_dab_x(DP_BrushDab *dab);

int DP_brush_dab_y(DP_BrushDab *dab);

uint8_t DP_brush_dab_opacity(DP_BrushDab *dab);


DP_BrushDab *DP_classic_brush_dab_base(DP_ClassicBrushDab *dab);

int DP_classic_brush_dab_x(DP_ClassicBrushDab *dab);

int DP_classic_brush_dab_y(DP_ClassicBrushDab *dab);

int DP_classic_brush_dab_size(DP_ClassicBrushDab *dab);

uint8_t DP_classic_brush_dab_hardness(DP_ClassicBrushDab *dab);

uint8_t DP_classic_brush_dab_opacity(DP_ClassicBrushDab *dab);

DP_ClassicBrushDab *DP_classic_brush_dab_at(DP_ClassicBrushDab *dabs, int i);


DP_BrushDab *DP_pixel_brush_dab_base(DP_PixelBrushDab *dab);

int DP_pixel_brush_dab_x(DP_PixelBrushDab *dab);

int DP_pixel_brush_dab_y(DP_PixelBrushDab *dab);

int DP_pixel_brush_dab_size(DP_PixelBrushDab *dab);

uint8_t DP_pixel_brush_dab_opacity(DP_PixelBrushDab *dab);

DP_PixelBrushDab *DP_pixel_brush_dab_at(DP_PixelBrushDab *dabs, int i);


DP_MsgDrawDabs *DP_msg_draw_dabs_cast(DP_Message *msg);

DP_MsgDrawDabsClassic *DP_msg_draw_dabs_cast_classic(DP_MsgDrawDabs *mdd);

DP_MsgDrawDabsPixel *DP_msg_draw_dabs_cast_pixel(DP_MsgDrawDabs *mdd);

int DP_msg_draw_dabs_layer_id(DP_MsgDrawDabs *mdd);

int DP_msg_draw_dabs_origin_x(DP_MsgDrawDabs *mdd);

int DP_msg_draw_dabs_origin_y(DP_MsgDrawDabs *mdd);

uint32_t DP_msg_draw_dabs_color(DP_MsgDrawDabs *mdd);

int DP_msg_draw_dabs_blend_mode(DP_MsgDrawDabs *mdd);

bool DP_msg_draw_dabs_indirect(DP_MsgDrawDabs *mdd);

void DP_msg_draw_dabs_bounds(DP_MsgDrawDabs *mdd, int *out_x, int *out_y,
                             int *out_width, int *out_height);


DP_Message *DP_msg_draw_dabs_classic_new(unsigned int context_id, int layer_id,
                                         int origin_x, int origin_y,
                                         uint32_t color, int blend_mode,
                                         int dab_count);

DP_Message *DP_msg_draw_dabs_classic_deserialize(unsigned int context_id,
                                                 const unsigned char *buffer,
                                                 size_t length);

DP_MsgDrawDabsClassic *DP_msg_draw_dabs_classic_cast(DP_Message *msg);


DP_MsgDrawDabs *DP_msg_draw_dabs_classic_base(DP_MsgDrawDabsClassic *mddc);

int DP_msg_draw_dabs_classic_layer_id(DP_MsgDrawDabsClassic *mddc);

int DP_msg_draw_dabs_classic_origin_x(DP_MsgDrawDabsClassic *mddc);

int DP_msg_draw_dabs_classic_origin_y(DP_MsgDrawDabsClassic *mddc);

uint32_t DP_msg_draw_dabs_classic_color(DP_MsgDrawDabsClassic *mddc);

int DP_msg_draw_dabs_classic_blend_mode(DP_MsgDrawDabsClassic *mddc);

DP_ClassicBrushDab *DP_msg_draw_dabs_classic_dabs(DP_MsgDrawDabsClassic *mddc,
                                                  int *out_count);

bool DP_msg_draw_dabs_classic_indirect(DP_MsgDrawDabsClassic *mddc);

void DP_msg_draw_dabs_classic_bounds(DP_MsgDrawDabsClassic *mddc, int *out_x,
                                     int *out_y, int *out_width,
                                     int *out_height);


DP_Message *DP_msg_draw_dabs_pixel_new(int type, unsigned int context_id,
                                       int layer_id, int origin_x, int origin_y,
                                       uint32_t color, int blend_mode,
                                       int dab_count);

DP_Message *DP_msg_draw_dabs_pixel_deserialize(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length);

DP_Message *DP_msg_draw_dabs_pixel_square_deserialize(
    unsigned int context_id, const unsigned char *buffer, size_t length);


DP_MsgDrawDabsPixel *DP_msg_draw_dabs_pixel_cast(DP_Message *msg);


DP_MsgDrawDabs *DP_msg_draw_dabs_pixel_base(DP_MsgDrawDabsPixel *mddp);

int DP_msg_draw_dabs_pixel_layer_id(DP_MsgDrawDabsPixel *mddp);

int DP_msg_draw_dabs_pixel_origin_x(DP_MsgDrawDabsPixel *mddp);

int DP_msg_draw_dabs_pixel_origin_y(DP_MsgDrawDabsPixel *mddp);

uint32_t DP_msg_draw_dabs_pixel_color(DP_MsgDrawDabsPixel *mddp);

int DP_msg_draw_dabs_pixel_blend_mode(DP_MsgDrawDabsPixel *mddp);

DP_PixelBrushDab *DP_msg_draw_dabs_pixel_dabs(DP_MsgDrawDabsPixel *mddp,
                                              int *out_count);

bool DP_msg_draw_dabs_pixel_indirect(DP_MsgDrawDabsPixel *mddp);

void DP_msg_draw_dabs_pixel_bounds(DP_MsgDrawDabsPixel *mddp, int *out_x,
                                   int *out_y, int *out_width, int *out_height);


#endif
