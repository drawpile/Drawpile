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
#ifndef DPMSG_PUT_TILE_H
#define DPMSG_PUT_TILE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgPutTile DP_MsgPutTile;

DP_Message *DP_msg_put_tile_new(unsigned int context_id, int layer_id,
                                int sublayer_id, int x, int y, int repeat,
                                const unsigned char *image, size_t image_size);

DP_Message *DP_msg_put_tile_deserialize(unsigned int context_id,
                                        const unsigned char *buffer,
                                        size_t length);

DP_MsgPutTile *DP_msg_put_tile_cast(DP_Message *msg);


int DP_msg_put_tile_layer_id(DP_MsgPutTile *mpt);
int DP_msg_put_tile_sublayer_id(DP_MsgPutTile *mpt);

int DP_msg_put_tile_x(DP_MsgPutTile *mpt);
int DP_msg_put_tile_y(DP_MsgPutTile *mpt);

int DP_msg_put_tile_repeat(DP_MsgPutTile *mpt);

bool DP_msg_put_tile_color(DP_MsgPutTile *mpt, uint32_t *out_color);
const unsigned char *DP_msg_put_tile_image(DP_MsgPutTile *mpt,
                                           size_t *out_image_size);


#endif
