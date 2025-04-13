// SPDX-License-Identifier: GPL-3.0-or-later
#include "selection_set.h"
#include "canvas_diff.h"
#include "selection.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_SelectionSet {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    union {
        DP_Selection *sel;
    } elements[];
};

struct DP_TransientSelectionSet {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Selection *sel;
        DP_TransientSelection *tsel;
    } elements[];
};

#else

struct DP_SelectionSet {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Selection *sel;
        DP_TransientSelection *tsel;
    } elements[];
};

#endif


static size_t selection_set_size(int count)
{
    return DP_FLEX_SIZEOF(DP_SelectionSet, elements, DP_int_to_size(count));
}

static void *allocate_selection_set(bool transient, int count)
{
    DP_TransientSelectionSet *tll = DP_malloc(selection_set_size(count));
    DP_atomic_set(&tll->refcount, 1);
    tll->transient = transient;
    tll->count = count;
    return tll;
}


DP_SelectionSet *DP_selection_set_incref(DP_SelectionSet *ss)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    DP_atomic_inc(&ss->refcount);
    return ss;
}

DP_SelectionSet *DP_selection_set_incref_nullable(DP_SelectionSet *ss_or_null)
{
    return ss_or_null ? DP_selection_set_incref(ss_or_null) : NULL;
}

void DP_selection_set_decref(DP_SelectionSet *ss)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    if (DP_atomic_dec(&ss->refcount)) {
        int count = ss->count;
        for (int i = 0; i < count; ++i) {
            DP_selection_decref_nullable(ss->elements[i].sel);
        }
        DP_free(ss);
    }
}

void DP_selection_set_decref_nullable(DP_SelectionSet *ss_or_null)
{
    if (ss_or_null) {
        DP_selection_set_decref(ss_or_null);
    }
}

int DP_selection_set_refcount(DP_SelectionSet *ss)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    return DP_atomic_get(&ss->refcount);
}

bool DP_selection_set_transient(DP_SelectionSet *ss)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    return ss->transient;
}

int DP_selection_set_count(DP_SelectionSet *ss)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    return ss->count;
}

DP_Selection *DP_selection_set_search_noinc(DP_SelectionSet *ss,
                                            unsigned int context_id,
                                            int selection_id)
{
    int index = DP_selection_set_search_index(ss, context_id, selection_id);
    return index < 0 ? NULL : ss->elements[index].sel;
}

int DP_selection_set_search_index(DP_SelectionSet *ss, unsigned int context_id,
                                  int selection_id)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    int count = ss->count;
    for (int i = 0; i < count; ++i) {
        DP_Selection *sel = ss->elements[i].sel;
        if (DP_selection_context_id(sel) == context_id
            && DP_selection_id(sel) == selection_id) {
            return i;
        }
    }
    return -1;
}

DP_Selection *DP_selection_set_at_noinc(DP_SelectionSet *ss, int index)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ss->count);
    return ss->elements[index].sel;
}

DP_TransientSelectionSet *DP_selection_set_resize(DP_SelectionSet *ss, int top,
                                                  int right, int bottom,
                                                  int left)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    int count = ss->count;
    DP_TransientSelectionSet *tss = allocate_selection_set(true, count);
    for (int i = 0, j = 0; i < count; ++i) {
        DP_Selection *sel =
            DP_selection_resize(ss->elements[i].sel, top, right, bottom, left);
        if (sel) {
            tss->elements[j].sel = sel;
            ++j;
        }
        else {
            --tss->count;
        }
    }

    if (tss->count == 0) {
        DP_transient_selection_set_decref(tss);
        return NULL;
    }
    else {
        return tss;
    }
}

static bool matches_ids(DP_Selection *sel, unsigned int context_id,
                        int selection_id)
{
    return DP_selection_context_id(sel) == context_id
        && (selection_id == 0 || DP_selection_id(sel) == selection_id);
}

static DP_TransientSelectionSet *actually_remove(DP_SelectionSet *ss,
                                                 unsigned int context_id,
                                                 int selection_id,
                                                 int first_removed, int count)
{
    DP_TransientSelectionSet *tss = allocate_selection_set(true, count - 1);
    // Copy over the elements we already looked at.
    for (int i = 0; i < first_removed; ++i) {
        tss->elements[i].sel = DP_selection_incref(ss->elements[i].sel);
    }
    // Keep removing further matching elements.
    for (int i = first_removed + 1; i < count; ++i) {
        DP_Selection *sel = ss->elements[i].sel;
        if (matches_ids(sel, context_id, selection_id)) {
            --tss->count;
        }
        else {
            int j = i - (count - tss->count);
            tss->elements[j].sel = DP_selection_incref(sel);
        }
    }
    return tss;
}

DP_TransientSelectionSet *DP_selection_set_remove(DP_SelectionSet *ss,
                                                  unsigned int context_id,
                                                  int selection_id)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    int count = ss->count;
    for (int i = 0; i < count; ++i) {
        DP_Selection *sel = ss->elements[i].sel;
        if (matches_ids(sel, context_id, selection_id)) {
            return actually_remove(ss, context_id, selection_id, i, count);
        }
    }
    return NULL;
}

static DP_Selection *get_active(DP_SelectionSet *ss_or_null,
                                unsigned int context_id, int selection_id)
{
    return ss_or_null && selection_id > 0
             ? DP_selection_set_search_noinc(ss_or_null, context_id,
                                             selection_id)
             : NULL;
}

void DP_selection_set_diff_nullable(DP_SelectionSet *ss_or_null,
                                    unsigned int active_context_id,
                                    int active_selection_id,
                                    DP_SelectionSet *prev_or_null,
                                    unsigned int prev_active_context_id,
                                    int prev_active_selection_id,
                                    DP_CanvasDiff *diff)
{
    DP_Selection *sel =
        get_active(ss_or_null, active_context_id, active_selection_id);
    DP_Selection *prev_sel = get_active(prev_or_null, prev_active_context_id,
                                        prev_active_selection_id);
    DP_selection_diff_nullable(sel, prev_sel, diff);
}


DP_TransientSelectionSet *DP_transient_selection_set_new_init(int reserve)
{
    DP_TransientSelectionSet *tss = allocate_selection_set(true, reserve);
    for (int i = 0; i < reserve; ++i) {
        tss->elements[i].sel = NULL;
    }
    return tss;
}

DP_TransientSelectionSet *DP_transient_selection_set_new(DP_SelectionSet *ss,
                                                         int reserve)
{
    DP_ASSERT(ss);
    DP_ASSERT(DP_atomic_get(&ss->refcount) > 0);
    DP_ASSERT(!ss->transient);
    DP_ASSERT(reserve >= 0);
    int count = ss->count;
    DP_TransientSelectionSet *tss =
        allocate_selection_set(true, count + reserve);
    for (int i = 0; i < count; ++i) {
        tss->elements[i].sel = DP_selection_incref(ss->elements[i].sel);
    }
    for (int i = 0; i < reserve; ++i) {
        tss->elements[count + i].sel = NULL;
    }
    return tss;
}

DP_TransientSelectionSet *
DP_transient_selection_set_reserve(DP_TransientSelectionSet *tss, int reserve)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in selection set", reserve);
    if (reserve > 0) {
        int old_count = tss->count;
        int new_count = old_count + reserve;
        tss = DP_realloc(tss, selection_set_size(new_count));
        tss->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tss->elements[i].sel = NULL;
        }
    }
    return tss;
}

DP_TransientSelectionSet *
DP_transient_selection_set_incref(DP_TransientSelectionSet *tss)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    return (DP_TransientSelectionSet *)DP_selection_set_incref(
        (DP_SelectionSet *)tss);
}

void DP_transient_selection_set_decref(DP_TransientSelectionSet *tss)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_selection_set_decref((DP_SelectionSet *)tss);
}

int DP_transient_selection_set_refcount(DP_TransientSelectionSet *tss)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    return DP_selection_set_refcount((DP_SelectionSet *)tss);
}

DP_SelectionSet *
DP_transient_selection_set_persist(DP_TransientSelectionSet *tss)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);

    int i = 0;
    while (i < tss->count) {
        if (DP_selection_transient(tss->elements[i].sel)
            && !DP_transient_selection_persist(tss->elements[i].tsel)) {
            DP_transient_selection_set_delete_at(tss, i);
        }
        else {
            ++i;
        }
    }

    if (tss->count > 0) {
        tss->transient = false;
        return (DP_SelectionSet *)tss;
    }
    else {
        return NULL;
    }
}

int DP_transient_selection_set_count(DP_TransientSelectionSet *tss)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    return tss->count;
}

DP_TransientSelection *
DP_transient_selection_set_transient_at_noinc(DP_TransientSelectionSet *tss,
                                              int index)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tss->count);
    DP_ASSERT(tss->elements[index].sel);
    DP_Selection *sel = tss->elements[index].sel;
    if (DP_selection_transient(sel)) {
        return (DP_TransientSelection *)sel;
    }
    else {
        DP_TransientSelection *tsel = DP_transient_selection_new(sel);
        tss->elements[index].tsel = tsel;
        DP_selection_decref(sel);
        return tsel;
    }
}

void DP_transient_selection_set_insert_at_noinc(DP_TransientSelectionSet *tss,
                                                int index, DP_Selection *sel)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tss->count);
    DP_ASSERT(!tss->elements[index].sel);
    DP_ASSERT(sel);
    tss->elements[index].sel = sel;
}

void DP_transient_selection_set_insert_transient_at_noinc(
    DP_TransientSelectionSet *tss, int index, DP_TransientSelection *tsel)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tss->count);
    DP_ASSERT(!tss->elements[index].sel);
    DP_ASSERT(tsel);
    tss->elements[index].tsel = tsel;
}

void DP_transient_selection_set_replace_at_noinc(DP_TransientSelectionSet *tss,
                                                 int index, DP_Selection *sel)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tss->count);
    DP_ASSERT(tss->elements[index].sel);
    DP_ASSERT(sel);
    DP_ASSERT(tss->elements[index].sel != sel);
    DP_Selection **pp = &tss->elements[index].sel;
    DP_selection_decref(*pp);
    *pp = sel;
}

void DP_transient_selection_set_delete_at(DP_TransientSelectionSet *tss,
                                          int index)
{
    DP_ASSERT(tss);
    DP_ASSERT(DP_atomic_get(&tss->refcount) > 0);
    DP_ASSERT(tss->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tss->count);
    DP_ASSERT(tss->elements[index].sel);
    DP_selection_decref(tss->elements[index].sel);
    int new_count = --tss->count;
    memmove(&tss->elements[index], &tss->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tss->elements[0]));
}
