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
#include "access_tier.h"
#include <dpcommon/common.h>

typedef struct DP_AccessTierAttributes {
    const char *enum_name;
    const char *name;
} DP_AccessTierAttributes;

static const DP_AccessTierAttributes access_tier_attributes[] = {
    [DP_ACCESS_TIER_OPERATOR] = {"DP_ACCESS_TIER_OPERATOR", "op"},
    [DP_ACCESS_TIER_TRUSTED] = {"DP_ACCESS_TIER_TRUSTED", "trusted"},
    [DP_ACCESS_TIER_AUTHENTICATED] = {"DP_ACCESS_TIER_AUTHENTICATED ", "auth"},
    [DP_ACCESS_TIER_GUEST] = {"DP_ACCESS_TIER_GUEST", "guest"},
};

int DP_access_tier_clamp(int tier)
{
    if (tier < 0) {
        return 0;
    }
    else if (tier >= DP_ACCESS_TIER_COUNT) {
        return DP_ACCESS_TIER_COUNT - 1;
    }
    else {
        return tier;
    }
}

static const DP_AccessTierAttributes *access_tier_at(int tier)
{
    if (tier >= 0 && tier < DP_ACCESS_TIER_COUNT) {
        return &access_tier_attributes[tier];
    }
    else {
        DP_error_set("Unknown access tier: %d", tier);
        return NULL;
    }
}

const char *DP_access_tier_enum_name(int tier)
{
    const DP_AccessTierAttributes *attributes = access_tier_at(tier);
    return attributes ? attributes->enum_name : NULL;
}

const char *DP_access_tier_name(int tier)
{
    const DP_AccessTierAttributes *attributes = access_tier_at(tier);
    return attributes ? attributes->name : NULL;
}
