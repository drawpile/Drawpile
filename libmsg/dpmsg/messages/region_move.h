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
#ifndef DPMSG_REGION_MOVE_H
#define DPMSG_REGION_MOVE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgRegionMove DP_MsgRegionMove;

DP_Message *DP_msg_region_move_new(unsigned int context_id, int layer_id,
                                   int src_x, int src_y, int src_width,
                                   int src_height, int x1, int y1, int x2,
                                   int y2, int x3, int y3, int x4, int y4,
                                   const unsigned char *mask, size_t mask_size);

DP_Message *DP_msg_region_move_deserialize(unsigned int context_id,
                                           const unsigned char *buffer,
                                           size_t length);

DP_MsgRegionMove *DP_msg_region_move_cast(DP_Message *msg);


int DP_msg_region_move_layer_id(DP_MsgRegionMove *mrm);

void DP_msg_region_move_src_rect(DP_MsgRegionMove *mrm, int *out_src_x,
                                 int *out_src_y, int *out_src_width,
                                 int *out_src_height);

void DP_msg_region_move_dst_quad(DP_MsgRegionMove *mrm, int *out_x1,
                                 int *out_y1, int *out_x2, int *out_y2,
                                 int *out_x3, int *out_y3, int *out_x4,
                                 int *out_y4);

const unsigned char *DP_msg_region_move_mask(DP_MsgRegionMove *mrm,
                                             size_t *out_mask_size);


#endif
