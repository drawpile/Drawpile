// SPDX-License-Identifier: GPL-3.0-or-later
#include "selection.h"
#include "layer_content.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/geom.h>


struct DP_Selection {
    DP_Atomic refcount;
    unsigned int context_id;
    int selection_id;
    DP_Rect bounds;
    DP_LayerContent *lc;
};


DP_Selection *DP_selection_new_init(unsigned int context_id, int selection_id,
                                    DP_LayerContent *lc, const DP_Rect *bounds)
{
    DP_ASSERT(lc);
    DP_ASSERT(bounds);
    DP_ASSERT(DP_rect_width(*bounds) > 0);
    DP_ASSERT(DP_rect_height(*bounds) > 0);
    DP_ASSERT(DP_rect_left(*bounds) >= 0);
    DP_ASSERT(DP_rect_top(*bounds) >= 0);
    DP_ASSERT(DP_rect_right(*bounds) < DP_layer_content_width(lc));
    DP_ASSERT(DP_rect_bottom(*bounds) < DP_layer_content_height(lc));
    DP_Selection *sel = DP_malloc(sizeof(*sel));
    *sel = (DP_Selection){DP_ATOMIC_INIT(1), context_id, selection_id, *bounds,
                          lc};
    return sel;
}

DP_Selection *DP_selection_incref(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    DP_atomic_inc(&sel->refcount);
    return sel;
}

DP_Selection *DP_selection_incref_nullable(DP_Selection *sel_or_null)
{
    return sel_or_null ? DP_selection_incref(sel_or_null) : NULL;
}

void DP_selection_decref(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    if (DP_atomic_dec(&sel->refcount)) {
        DP_layer_content_decref(sel->lc);
        DP_free(sel);
    }
}

void DP_selection_decref_nullable(DP_Selection *sel_or_null)
{
    if (sel_or_null) {
        DP_selection_decref(sel_or_null);
    }
}

int DP_selection_refcount(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    return DP_atomic_get(&sel->refcount);
}

unsigned int DP_selection_context_id(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    return sel->context_id;
}

int DP_selection_id(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    return sel->selection_id;
}

const DP_Rect *DP_selection_bounds(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    return &sel->bounds;
}

DP_LayerContent *DP_selection_content_noinc(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    return sel->lc;
}

DP_Selection *DP_selection_resize(DP_Selection *sel, int top, int right,
                                  int bottom, int left)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    DP_TransientLayerContent *tlc = DP_layer_content_resize(
        sel->lc, sel->context_id, top, right, bottom, left);
    DP_Rect bounds;
    if (DP_transient_layer_content_bounds(tlc, &bounds)) {
        return DP_selection_new_init(
            DP_selection_context_id(sel), DP_selection_id(sel),
            DP_transient_layer_content_persist(tlc), &bounds);
    }
    else {
        DP_transient_layer_content_decref(tlc);
        return NULL;
    }
}
