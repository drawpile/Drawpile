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
#ifndef DPMSG_LAYER_CREATE_H
#define DPMSG_LAYER_CREATE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_MSG_LAYER_CREATE_FLAG_COPY   (1 << 0)
#define DP_MSG_LAYER_CREATE_FLAG_INSERT (1 << 1)
#define DP_MSG_LAYER_CREATE_FLAG_MASK \
    (DP_MSG_LAYER_CREATE_FLAG_COPY | DP_MSG_LAYER_CREATE_FLAG_INSERT)


typedef struct DP_MsgLayerCreate DP_MsgLayerCreate;

DP_Message *DP_msg_layer_create_new(unsigned int context_id, int layer_id,
                                    int source_id, uint32_t fill,
                                    unsigned int flags, const char *title,
                                    size_t title_length);

DP_Message *DP_msg_layer_create_deserialize(unsigned int context_id,
                                            const unsigned char *buffer,
                                            size_t length);

DP_MsgLayerCreate *DP_msg_layer_create_cast(DP_Message *msg);


int DP_msg_layer_create_layer_id(DP_MsgLayerCreate *mlc);

int DP_msg_layer_create_source_id(DP_MsgLayerCreate *mlc);

uint32_t DP_msg_layer_create_fill(DP_MsgLayerCreate *mlc);

unsigned int DP_msg_layer_create_flags(DP_MsgLayerCreate *mlc);

const char *DP_msg_layer_create_title(DP_MsgLayerCreate *mlc,
                                      size_t *out_length);

bool DP_msg_layer_create_id_valid(DP_MsgLayerCreate *mlc);


#endif
