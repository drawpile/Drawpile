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
#ifndef DPMSG_ACCESS_TIER_H
#define DPMSG_ACCESS_TIER_H

typedef enum DP_AccessTier {
    DP_ACCESS_TIER_OPERATOR = 0,
    DP_ACCESS_TIER_TRUSTED = 1,
    DP_ACCESS_TIER_AUTHENTICATED = 2,
    DP_ACCESS_TIER_GUEST = 3,
    DP_ACCESS_TIER_COUNT,
} DP_AccessTier;


int DP_access_tier_clamp(int tier);

const char *DP_access_tier_enum_name(int tier);

const char *DP_access_tier_name(int tier);


#endif
