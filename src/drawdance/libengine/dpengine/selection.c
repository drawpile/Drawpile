// SPDX-License-Identifier: GPL-3.0-or-later
#include "selection.h"
#include "canvas_diff.h"
#include "layer_content.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/geom.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_Selection {
    DP_Atomic refcount;
    const bool transient;
    const unsigned int context_id;
    const int selection_id;
    const DP_Rect bounds;
    DP_LayerContent *const lc;
};

struct DP_TransientSelection {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    int selection_id;
    DP_Rect bounds;
    union {
        DP_LayerContent *lc;
        DP_TransientLayerContent *tlc;
    };
};

#else

struct DP_Selection {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    int selection_id;
    DP_Rect bounds;
    union {
        DP_LayerContent *lc;
        DP_TransientLayerContent *tlc;
    };
};

#endif


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
    DP_TransientSelection *tsel = DP_malloc(sizeof(*tsel));
    *tsel = (DP_TransientSelection){DP_ATOMIC_INIT(1), false,   context_id,
                                    selection_id,      *bounds, {.lc = lc}};
    return (DP_Selection *)tsel;
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

bool DP_selection_transient(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    return sel->transient;
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
    if (sel->transient) {
        return DP_transient_selection_bounds((DP_TransientSelection *)sel);
    }
    else {
        return &sel->bounds;
    }
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
    if (DP_transient_layer_content_bounds(tlc, true, &bounds)) {
        return DP_selection_new_init(
            DP_selection_context_id(sel), DP_selection_id(sel),
            DP_transient_layer_content_persist(tlc), &bounds);
    }
    else {
        DP_transient_layer_content_decref(tlc);
        return NULL;
    }
}

void DP_selection_diff_nullable(DP_Selection *sel_or_null,
                                DP_Selection *prev_or_null, DP_CanvasDiff *diff)
{
    if (sel_or_null) {
        if (prev_or_null) {
            DP_LayerContent *lc = sel_or_null->lc;
            DP_LayerContent *prev_lc = prev_or_null->lc;
            if (lc != prev_lc) {
                DP_layer_content_diff_selection(lc, prev_lc, diff);
            }
        }
        else {
            DP_layer_content_diff_mark(sel_or_null->lc, diff);
        }
    }
    else if (prev_or_null) {
        DP_layer_content_diff_mark(prev_or_null->lc, diff);
    }
}


DP_TransientSelection *DP_transient_selection_new(DP_Selection *sel)
{
    DP_ASSERT(sel);
    DP_ASSERT(DP_atomic_get(&sel->refcount) > 0);
    DP_ASSERT(!sel->transient);
    DP_ASSERT(!DP_layer_content_transient(sel->lc));
    DP_TransientSelection *tsel = DP_malloc(sizeof(*tsel));
    *tsel = (DP_TransientSelection){
        DP_ATOMIC_INIT(1), true,
        sel->context_id,   sel->selection_id,
        sel->bounds,       {.lc = DP_layer_content_incref(sel->lc)}};
    return tsel;
}

DP_TransientSelection *DP_transient_selection_new_init(unsigned int context_id,
                                                       int selection_id,
                                                       int width, int height)
{
    DP_TransientSelection *tsel = DP_malloc(sizeof(*tsel));
    *tsel = (DP_TransientSelection){
        DP_ATOMIC_INIT(1),
        true,
        context_id,
        selection_id,
        (DP_Rect){0, 0, -1, -1},
        {.tlc = DP_transient_layer_content_new_init(width, height, NULL)}};
    return tsel;
}

DP_TransientSelection *
DP_transient_selection_incref(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    return (DP_TransientSelection *)DP_selection_incref((DP_Selection *)tsel);
}

void DP_transient_selection_decref(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    DP_selection_decref((DP_Selection *)tsel);
}

int DP_transient_selection_refcount(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    return DP_selection_refcount((DP_Selection *)tsel);
}

static bool transient_selection_update_bounds(DP_TransientSelection *tsel)
{
    if (DP_transient_layer_content_bounds(tsel->tlc, true, &tsel->bounds)) {
        return true;
    }
    else {
        tsel->bounds = (DP_Rect){0, 0, -1, -1};
        return false;
    }
}

DP_Selection *DP_transient_selection_persist(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    if (DP_layer_content_transient(tsel->lc)) {
        if (transient_selection_update_bounds(tsel)) {
            DP_transient_layer_content_persist(tsel->tlc);
        }
        else {
            return NULL;
        }
    }
    tsel->transient = false;
    return (DP_Selection *)tsel;
}

unsigned int DP_transient_selection_context_id(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    return tsel->context_id;
}

int DP_transient_selection_id(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    return tsel->selection_id;
}

const DP_Rect *DP_transient_selection_bounds(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    if (DP_layer_content_transient(tsel->lc)) {
        // Transient layer bounds may change from under us at any time, so we
        // have to recalculate them to be safe.
        transient_selection_update_bounds(tsel);
    }
    return &tsel->bounds;
}

DP_LayerContent *
DP_transient_selection_content_noinc(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    return tsel->lc;
}

DP_TransientLayerContent *
DP_transient_selection_transient_content_noinc(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    if (DP_layer_content_transient(tsel->lc)) {
        return tsel->tlc;
    }
    else {
        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_new(tsel->lc);
        DP_layer_content_decref(tsel->lc);
        tsel->tlc = tlc;
        return tlc;
    }
}
