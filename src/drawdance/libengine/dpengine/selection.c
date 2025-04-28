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
    DP_LayerContent *const lc;
};

struct DP_TransientSelection {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    int selection_id;
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
    union {
        DP_LayerContent *lc;
        DP_TransientLayerContent *tlc;
    };
};

#endif


DP_Selection *DP_selection_new_init(unsigned int context_id, int selection_id,
                                    DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_TransientSelection *tsel = DP_malloc(sizeof(*tsel));
    *tsel = (DP_TransientSelection){
        DP_ATOMIC_INIT(1), false, context_id, selection_id, {.lc = lc}};
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
    if (DP_transient_layer_content_has_content(tlc)) {
        return DP_selection_new_init(
            DP_selection_context_id(sel), DP_selection_id(sel),
            DP_transient_layer_content_persist_mask(tlc));
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
    *tsel = (DP_TransientSelection){DP_ATOMIC_INIT(1),
                                    true,
                                    sel->context_id,
                                    sel->selection_id,
                                    {.lc = DP_layer_content_incref(sel->lc)}};
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

DP_Selection *DP_transient_selection_persist(DP_TransientSelection *tsel)
{
    DP_ASSERT(tsel);
    DP_ASSERT(DP_atomic_get(&tsel->refcount) > 0);
    DP_ASSERT(tsel->transient);
    if (DP_layer_content_transient(tsel->lc)) {
        DP_TransientLayerContent *tlc = tsel->tlc;
        if (DP_transient_layer_content_has_content(tlc)) {
            DP_transient_layer_content_persist_mask(tlc);
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
