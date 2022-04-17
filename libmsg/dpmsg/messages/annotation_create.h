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
#ifndef DPMSG_ANNOTATION_CREATE_H
#define DPMSG_ANNOTATION_CREATE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgAnnotationCreate DP_MsgAnnotationCreate;

DP_Message *DP_msg_annotation_create_new(unsigned int context_id,
                                         int annotation_id, int x, int y,
                                         int width, int height);

DP_Message *DP_msg_annotation_create_deserialize(unsigned int context_id,
                                                 const unsigned char *buffer,
                                                 size_t length);

DP_MsgAnnotationCreate *DP_msg_annotation_create_cast(DP_Message *msg);


int DP_msg_annotation_create_annotation_id(DP_MsgAnnotationCreate *mac);

int DP_msg_annotation_create_x(DP_MsgAnnotationCreate *mac);

int DP_msg_annotation_create_y(DP_MsgAnnotationCreate *mac);

int DP_msg_annotation_create_width(DP_MsgAnnotationCreate *mac);

int DP_msg_annotation_create_height(DP_MsgAnnotationCreate *mac);


#endif
