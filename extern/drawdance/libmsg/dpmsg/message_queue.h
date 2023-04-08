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
#ifndef DPMSG_MESSAGE_REF_QUEUE_H
#define DPMSG_MESSAGE_REF_QUEUE_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;
typedef struct DP_Queue DP_Queue;


void DP_message_queue_init(DP_Queue *queue, size_t initial_capacity);

void DP_message_queue_dispose(DP_Queue *queue);

DP_Message *DP_message_queue_push_noinc(DP_Queue *queue, DP_Message *msg);

DP_Message *DP_message_queue_push_inc(DP_Queue *queue, DP_Message *msg);

DP_Message *DP_message_queue_peek(DP_Queue *queue);

DP_Message *DP_message_queue_shift(DP_Queue *queue);


#endif
