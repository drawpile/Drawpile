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
#include "feature_levels.h"
#include "../access_tier.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/common.h>


struct DP_MsgFeatureLevels {
    uint8_t feature_tiers[DP_MSG_FEATURE_LEVELS_FEATURE_COUNT];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return DP_MSG_FEATURE_LEVELS_FEATURE_COUNT;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgFeatureLevels *mfl = DP_msg_feature_levels_cast(msg);
    DP_ASSERT(mfl);
    memcpy(data, mfl->feature_tiers, DP_MSG_FEATURE_LEVELS_FEATURE_COUNT);
    return DP_MSG_FEATURE_LEVELS_FEATURE_COUNT;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    static int alphabetic_feature_indexes[DP_MSG_FEATURE_LEVELS_FEATURE_COUNT] =
        {
            3, 6, 4, 7, 5, 0, 1, 2, 8,
        };
    static const char *names[DP_MSG_FEATURE_LEVELS_FEATURE_COUNT] = {
        "putimage",  "regionmove",       "resize", "background", "editlayers",
        "ownlayers", "createannotation", "laser",  "undo",
    };

    DP_MsgFeatureLevels *mfl = DP_msg_feature_levels_cast(msg);
    DP_ASSERT(mfl);

    for (int i = 0; i < DP_MSG_FEATURE_LEVELS_FEATURE_COUNT; ++i) {
        int feature_index = alphabetic_feature_indexes[i];
        int tier = DP_access_tier_clamp(mfl->feature_tiers[feature_index]);
        if (tier != 0) {
            DP_RETURN_UNLESS(DP_text_writer_write_string(
                writer, names[feature_index], DP_access_tier_name(tier)));
        }
    }

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgFeatureLevels *a = DP_msg_feature_levels_cast(msg);
    DP_MsgFeatureLevels *b = DP_msg_feature_levels_cast(other);
    return memcmp(a->feature_tiers, b->feature_tiers,
                  DP_MSG_FEATURE_LEVELS_FEATURE_COUNT)
        == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_feature_levels_new(
    unsigned int context_id,
    const unsigned char feature_tiers[DP_MSG_FEATURE_LEVELS_FEATURE_COUNT])
{
    DP_MsgFeatureLevels *mfl;
    DP_Message *msg = DP_message_new(DP_MSG_FEATURE_LEVELS, context_id,
                                     &methods, sizeof(*mfl));
    mfl = DP_message_internal(msg);
    memcpy(mfl->feature_tiers, feature_tiers,
           DP_MSG_FEATURE_LEVELS_FEATURE_COUNT);
    return msg;
}

DP_Message *DP_msg_feature_levels_deserialize(unsigned int context_id,
                                              const unsigned char *buffer,
                                              size_t length)
{
    if (length == DP_MSG_FEATURE_LEVELS_FEATURE_COUNT) {
        return DP_msg_feature_levels_new(context_id, buffer);
    }
    else {
        DP_error_set("Wrong length for FEATURE_LEVELS message: %zu", length);
        return NULL;
    }
}


DP_MsgFeatureLevels *DP_msg_feature_levels_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_FEATURE_LEVELS);
}


int DP_msg_feature_levels_feature_tier(DP_MsgFeatureLevels *mfl,
                                       int feature_index)
{
    DP_ASSERT(mfl);
    DP_ASSERT(feature_index >= 0);
    DP_ASSERT(feature_index < DP_MSG_FEATURE_LEVELS_FEATURE_COUNT);
    return mfl->feature_tiers[feature_index];
}
