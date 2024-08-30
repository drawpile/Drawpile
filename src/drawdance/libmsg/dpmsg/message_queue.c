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
#include "message_queue.h"
#include "message.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <dpcommon/vector.h>


#define ELEMENT_SIZE sizeof(DP_Message *)

void DP_message_queue_init(DP_Queue *queue, size_t initial_capacity)
{
    DP_ASSERT(queue);
    DP_queue_init(queue, initial_capacity, ELEMENT_SIZE);
}

static void dispose_message(void *pp)
{
    DP_message_decref(*((DP_Message **)pp));
}

void DP_message_queue_dispose(DP_Queue *queue)
{
    DP_ASSERT(queue);
    DP_queue_clear(queue, ELEMENT_SIZE, dispose_message);
    DP_queue_dispose(queue);
}

DP_Message *DP_message_queue_push_noinc(DP_Queue *queue, DP_Message *msg)
{
    DP_ASSERT(queue);
    DP_ASSERT(msg);
    DP_Message **pp = DP_queue_push(queue, ELEMENT_SIZE);
    *pp = msg;
    return msg;
}

DP_Message *DP_message_queue_push_inc(DP_Queue *queue, DP_Message *msg)
{
    DP_ASSERT(queue);
    DP_ASSERT(msg);
    return DP_message_queue_push_noinc(queue, DP_message_incref(msg));
}

DP_Message *DP_message_queue_peek(DP_Queue *queue)
{
    DP_Message **pp = DP_queue_peek(queue, ELEMENT_SIZE);
    return pp ? *pp : NULL;
}

DP_Message *DP_message_queue_shift(DP_Queue *queue)
{
    DP_ASSERT(queue);
    DP_Message **pp = DP_queue_peek(queue, ELEMENT_SIZE);
    if (pp) {
        DP_queue_shift(queue);
        return *pp;
    }
    else {
        return NULL;
    }
}


void DP_message_vector_init(DP_Vector *vec, size_t initial_capacity)
{
    DP_ASSERT(vec);
    DP_vector_init(vec, initial_capacity, ELEMENT_SIZE);
}

void DP_message_vector_dispose(DP_Vector *vec)
{
    DP_ASSERT(vec);
    DP_vector_clear_dispose(vec, ELEMENT_SIZE, dispose_message);
}

DP_Message *DP_message_vector_push_noinc(DP_Vector *vec, DP_Message *msg)
{
    DP_ASSERT(vec);
    DP_ASSERT(msg);
    DP_Message **pp = DP_vector_push(vec, ELEMENT_SIZE);
    *pp = msg;
    return msg;
}

DP_Message *DP_message_vector_push_inc(DP_Vector *vec, DP_Message *msg)
{
    DP_ASSERT(vec);
    DP_ASSERT(msg);
    return DP_message_vector_push_noinc(vec, DP_message_incref(msg));
}

DP_Message *DP_message_vector_at(DP_Vector *vec, size_t index)
{
    DP_ASSERT(vec);
    DP_ASSERT(index < vec->used);
    DP_Message **pp = DP_vector_at(vec, ELEMENT_SIZE, index);
    return *pp;
}
