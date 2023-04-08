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


// One indirect are per user (index 0 is kinda superflous, but that's okay.)
#define DP_AFFECTED_INDIRECT_AREAS_COUNT 256

#define DP_AFFECTED_AREA_PRINT(AA, TITLE, PRINT)                              \
    do {                                                                      \
        DP_AffectedArea *_aa = (AA);                                          \
        switch (_aa->domain) {                                                \
        case DP_AFFECTED_DOMAIN_USER_ATTRS:                                   \
            PRINT("%s affects local user", (TITLE));                          \
            break;                                                            \
        case DP_AFFECTED_DOMAIN_LAYER_ATTRS:                                  \
            PRINT("%s affects properties of layer %d", (TITLE),               \
                  _aa->affected_id);                                          \
            break;                                                            \
        case DP_AFFECTED_DOMAIN_ANNOTATIONS:                                  \
            PRINT("%s affects annotation %d", (TITLE), _aa->affected_id);     \
            break;                                                            \
        case DP_AFFECTED_DOMAIN_PIXELS: {                                     \
            DP_Rect _bounds = _aa->bounds;                                    \
            PRINT("%s affects pixels on layer %d, from (%d, %d) to (%d, %d)", \
                  (TITLE), _aa->affected_id, DP_rect_left(_bounds),           \
                  DP_rect_top(_bounds), DP_rect_right(_bounds),               \
                  DP_rect_bottom(_bounds));                                   \
            break;                                                            \
        }                                                                     \
        case DP_AFFECTED_DOMAIN_CANVAS_BACKGROUND:                            \
            PRINT("%s affects canvas background", (TITLE));                   \
            break;                                                            \
        case DP_AFFECTED_DOMAIN_DOCUMENT_METADATA:                            \
            PRINT("%s affects document metadata type %d", (TITLE),            \
                  _aa->affected_id);                                          \
            break;                                                            \
        case DP_AFFECTED_DOMAIN_TIMELINE:                                     \
            PRINT("%s affects timeline frame %d", (TITLE), _aa->affected_id); \
            break;                                                            \
        case DP_AFFECTED_DOMAIN_EVERYTHING:                                   \
            PRINT("%s affects everything", (TITLE));                          \
            break;                                                            \
        default:                                                              \
            PRINT("%s affects unknown domain %d", (TITLE), (int)_aa->domain); \
            break;                                                            \
        }                                                                     \
    } while (0)

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

typedef struct DP_IndirectArea {
    int layer_id;
    DP_Rect bounds;
} DP_IndirectArea;

typedef struct DP_AffectedIndirectAreas {
    DP_IndirectArea areas[DP_AFFECTED_INDIRECT_AREAS_COUNT];
} DP_AffectedIndirectAreas;


// If the message is an indirect stroke, the affected indirect areas for that
// user are amended with that stroke's bounds. If it's a pen up message, the
// collected bounds are used as the area for that message and the indirect area
// is cleared. Other messages don't affect the state of the indirect areas.
DP_AffectedArea DP_affected_area_make(DP_Message *msg,
                                      DP_AffectedIndirectAreas *aia);

bool DP_affected_area_concurrent_with(const DP_AffectedArea *aa,
                                      const DP_AffectedArea *other);

void DP_affected_indirect_areas_clear(DP_AffectedIndirectAreas *aia);


#endif
