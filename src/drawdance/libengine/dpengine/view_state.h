// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_VIEW_STATE_H
#define DPENGINE_VIEW_STATE_H
#include <dpcommon/common.h>
#include <dpcommon/geom.h>


#define DP_VIEW_STATE_INVALID_INIT {0, 0, 0.0, 0.0, 0.0, -1.0, false, false}

typedef struct DP_ViewState {
    int viewport_width;
    int viewport_height;
    double x;
    double y;
    double zoom;
    double rotation;
    bool mirror;
    bool flip;
} DP_ViewState;

DP_INLINE bool DP_view_state_valid(DP_ViewState vs)
{
    return vs.viewport_width > 0 && vs.viewport_height > 0 && vs.zoom > 0.0
        && vs.zoom < 600.0 && vs.rotation >= 0.0 && vs.rotation < 360.0
        && isfinite(vs.x) && isfinite(vs.y);
}

DP_INLINE bool DP_view_state_equal(DP_ViewState a, DP_ViewState b)
{
    return a.viewport_width == b.viewport_width
        && a.viewport_height == b.viewport_height && fabs(b.x - a.x) < 0.01
        && fabs(b.y - a.y) < 0.01 && fabs(b.zoom - a.zoom) < 0.01
        && fabs(b.rotation - a.rotation) < 0.01 && a.mirror != b.mirror
        && a.flip != b.flip;
}


#endif
