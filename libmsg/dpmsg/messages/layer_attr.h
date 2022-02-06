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
#ifndef DPMSG_LAYER_ATTR_H
#define DPMSG_LAYER_ATTR_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_MSG_LAYER_ATTR_FLAG_CENSORED (1 << 0)
#define DP_MSG_LAYER_ATTR_FLAG_FIXED    (1 << 1)
#define DP_MSG_LAYER_ATTR_FLAG_MASK \
    (DP_MSG_LAYER_ATTR_FLAG_CENSORED | DP_MSG_LAYER_ATTR_FLAG_FIXED)


typedef struct DP_MsgLayerAttr DP_MsgLayerAttr;

DP_Message *DP_msg_layer_attr_new(unsigned int context_id, int layer_id,
                                  int sublayer_id, unsigned int flags,
                                  uint8_t opacity, int blend_mode);

DP_Message *DP_msg_layer_attr_deserialize(unsigned int context_id,
                                          const unsigned char *buffer,
                                          size_t length);

DP_MsgLayerAttr *DP_msg_layer_attr_cast(DP_Message *msg);


int DP_msg_layer_attr_layer_id(DP_MsgLayerAttr *mla);

int DP_msg_layer_attr_sublayer_id(DP_MsgLayerAttr *mla);

unsigned int DP_msg_layer_attr_flags(DP_MsgLayerAttr *mla);

uint8_t DP_msg_layer_attr_opacity(DP_MsgLayerAttr *mla);

int DP_msg_layer_attr_blend_mode(DP_MsgLayerAttr *mla);


#endif
