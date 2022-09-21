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
#ifndef DP_AFFECTED_AREA
#define DP_AFFECTED_AREA
#include <dpcommon/common.h>
#include <dpcommon/geom.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Message DP_Message;


typedef enum DP_AffectedDomain {
    DP_AFFECTED_DOMAIN_USER_ATTRS,
    DP_AFFECTED_DOMAIN_LAYER_ATTRS,
    DP_AFFECTED_DOMAIN_ANNOTATIONS,
    DP_AFFECTED_DOMAIN_PIXELS,
    DP_AFFECTED_DOMAIN_CANVAS_BACKGROUND,
    DP_AFFECTED_DOMAIN_DOCUMENT_METADATA,
    DP_AFFECTED_DOMAIN_TIMELINE,
    DP_AFFECTED_DOMAIN_EVERYTHING,
} DP_AffectedDomain;

typedef struct DP_AffectedArea {
    DP_AffectedDomain domain;
    int affected_id; // layer, annotation, field or frame id
    DP_Rect bounds;
} DP_AffectedArea;


DP_AffectedArea DP_affected_area_make(DP_Message *msg, DP_CanvasState *cs);

bool DP_affected_area_concurrent_with(const DP_AffectedArea *aa,
                                      const DP_AffectedArea *other);


#endif
