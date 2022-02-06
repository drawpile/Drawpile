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
#ifndef DPMSG_LAYER_DELETE_H
#define DPMSG_LAYER_DELETE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgLayerDelete DP_MsgLayerDelete;

DP_Message *DP_msg_layer_delete_new(unsigned int context_id, int layer_id,
                                    bool merge);

DP_Message *DP_msg_layer_delete_deserialize(unsigned int context_id,
                                            const unsigned char *buffer,
                                            size_t length);

DP_MsgLayerDelete *DP_msg_layer_delete_cast(DP_Message *msg);


int DP_msg_layer_delete_layer_id(DP_MsgLayerDelete *mld);

bool DP_msg_layer_delete_merge(DP_MsgLayerDelete *mld);


#endif
