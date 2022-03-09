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
#ifndef DPMSG_LAYER_ORDER_H
#define DPMSG_LAYER_ORDER_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgLayerOrder DP_MsgLayerOrder;

DP_Message *DP_msg_layer_order_new(unsigned int context_id, int count,
                                   int (*get_layer_id)(void *user, int i),
                                   void *user);

DP_Message *DP_msg_layer_order_deserialize(unsigned int context_id,
                                           const unsigned char *buffer,
                                           size_t length);

DP_MsgLayerOrder *DP_msg_layer_order_cast(DP_Message *msg);


int DP_msg_layer_order_layer_id_count(DP_MsgLayerOrder *mlo);

int DP_msg_layer_order_layer_id_at(DP_MsgLayerOrder *mlo, int index);


#endif
