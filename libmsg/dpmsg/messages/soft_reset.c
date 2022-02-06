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
#include "soft_reset.h"
#include "../message.h"
#include "zero_length.h"
#include <dpcommon/common.h>


DP_Message *DP_msg_soft_reset_new(unsigned int context_id)
{
    return DP_zero_length_new(DP_MSG_SOFT_RESET, context_id);
}

DP_Message *DP_msg_soft_reset_deserialize(unsigned int context_id,
                                          const unsigned char *buffer,
                                          size_t length)
{
    return DP_zero_length_deserialize(DP_MSG_SOFT_RESET, context_id, buffer,
                                      length);
}


DP_MsgSoftReset *DP_msg_soft_reset_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_SOFT_RESET);
}
