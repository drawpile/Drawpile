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
#ifndef DPMSG_CANVAS_RESIZE_H
#define DPMSG_CANVAS_RESIZE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgCanvasResize DP_MsgCanvasResize;

DP_Message *DP_msg_canvas_resize_new(unsigned int context_id, int top,
                                     int right, int bottom, int left);

DP_Message *DP_msg_canvas_resize_deserialize(unsigned int context_id,
                                             const unsigned char *buffer,
                                             size_t length);

DP_MsgCanvasResize *DP_msg_canvas_resize_cast(DP_Message *msg);

int DP_msg_canvas_resize_top(DP_MsgCanvasResize *mcr);
int DP_msg_canvas_resize_right(DP_MsgCanvasResize *mcr);
int DP_msg_canvas_resize_bottom(DP_MsgCanvasResize *mcr);
int DP_msg_canvas_resize_left(DP_MsgCanvasResize *mcr);

void DP_msg_canvas_resize_dimensions(DP_MsgCanvasResize *mcr, int *out_top,
                                     int *out_right, int *out_bottom,
                                     int *out_left);


#endif
