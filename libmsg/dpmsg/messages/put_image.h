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
#ifndef DPMSG_PUT_IMAGE_H
#define DPMSG_PUT_IMAGE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgPutImage DP_MsgPutImage;

DP_Message *DP_msg_put_image_new(unsigned int context_id, int layer_id,
                                 int blend_mode, int x, int y, int width,
                                 int height, const unsigned char *image,
                                 size_t image_size);

DP_Message *DP_msg_put_image_deserialize(unsigned int context_id,
                                         const unsigned char *buffer,
                                         size_t length);

DP_MsgPutImage *DP_msg_put_image_cast(DP_Message *msg);


int DP_msg_put_image_layer_id(DP_MsgPutImage *mpi);

int DP_msg_put_image_blend_mode(DP_MsgPutImage *mpi);

int DP_msg_put_image_x(DP_MsgPutImage *mpi);
int DP_msg_put_image_y(DP_MsgPutImage *mpi);
int DP_msg_put_image_width(DP_MsgPutImage *mpi);
int DP_msg_put_image_height(DP_MsgPutImage *mpi);

const unsigned char *DP_msg_put_image_image(DP_MsgPutImage *mpi,
                                            size_t *out_image_size);


#endif
