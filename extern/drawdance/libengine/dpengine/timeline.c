/*
 * Copyright (C) 2022 askmeaboufoom
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
#include "timeline.h"
#include "frame.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_Timeline {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    struct {
        DP_Frame *const frame;
    } elements[];
};

struct DP_TransientTimeline {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Frame *frame;
        DP_TransientFrame *transient_frame;
    } elements[];
};

#else

struct DP_Timeline {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Frame *frame;
        DP_TransientFrame *transient_frame;
    } elements[];
};

#endif


static size_t timeline_size(int count)
{
    return DP_FLEX_SIZEOF(DP_Timeline, elements, DP_int_to_size(count));
}

static void *allocate_timeline(bool transient, int count)
{
    DP_TransientTimeline *ttl = DP_malloc(timeline_size(count));
    DP_atomic_set(&ttl->refcount, 1);
    ttl->transient = transient;
    ttl->count = count;
    return ttl;
}


DP_Timeline *DP_timeline_new(void)
{
    return allocate_timeline(false, 0);
}

DP_Timeline *DP_timeline_incref(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    DP_atomic_inc(&tl->refcount);
    return tl;
}

DP_Timeline *DP_timeline_incref_nullable(DP_Timeline *tl_or_null)
{
    return tl_or_null ? DP_timeline_incref(tl_or_null) : NULL;
}

void DP_timeline_decref(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    if (DP_atomic_dec(&tl->refcount)) {
        int count = tl->count;
        for (int i = 0; i < count; ++i) {
            DP_frame_decref_nullable(tl->elements[i].frame);
        }
        DP_free(tl);
    }
}

void DP_timeline_decref_nullable(DP_Timeline *tl_or_null)
{
    if (tl_or_null) {
        DP_timeline_decref(tl_or_null);
    }
}

int DP_timeline_refcount(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    return tl->refcount;
}

bool DP_timeline_transient(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    return tl->transient;
}

int DP_timeline_frame_count(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    return tl->count;
}

DP_Frame *DP_timeline_frame_at_noinc(DP_Timeline *tl, int index)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tl->count);
    return tl->elements[index].frame;
}


DP_TransientTimeline *DP_transient_timeline_new(DP_Timeline *tl, int reserve)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    DP_ASSERT(!tl->transient);
    DP_ASSERT(reserve >= 0);
    int count = tl->count;
    DP_TransientTimeline *ttl = allocate_timeline(true, count + reserve);
    for (int i = 0; i < count; ++i) {
        ttl->elements[i].frame = DP_frame_incref(tl->elements[i].frame);
    }
    for (int i = 0; i < reserve; ++i) {
        ttl->elements[count + i].frame = NULL;
    }
    return ttl;
}

DP_TransientTimeline *DP_transient_timeline_reserve(DP_TransientTimeline *ttl,
                                                    int reserve)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in timeline", reserve);
    if (reserve > 0) {
        int old_count = ttl->count;
        int new_count = old_count + reserve;
        ttl = DP_realloc(ttl, timeline_size(new_count));
        ttl->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            ttl->elements[i].frame = NULL;
        }
    }
    return ttl;
}

DP_TransientTimeline *DP_transient_timeline_incref(DP_TransientTimeline *ttl)
{
    return (DP_TransientTimeline *)DP_timeline_incref((DP_Timeline *)ttl);
}

void DP_transient_timeline_decref(DP_TransientTimeline *ttl)
{
    DP_timeline_decref((DP_Timeline *)ttl);
}

int DP_transient_timeline_refcount(DP_TransientTimeline *ttl)
{
    return DP_timeline_refcount((DP_Timeline *)ttl);
}

DP_Timeline *DP_transient_timeline_persist(DP_TransientTimeline *ttl)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    ttl->transient = false;
    int count = ttl->count;
    for (int i = 0; i < count; ++i) {
        DP_ASSERT(ttl->elements[i].frame);
        if (DP_frame_transient(ttl->elements[i].frame)) {
            DP_transient_frame_persist(ttl->elements[i].transient_frame);
        }
    }
    return (DP_Timeline *)ttl;
}

int DP_transient_timeline_frame_count(DP_TransientTimeline *ttl)
{
    return DP_timeline_frame_count((DP_Timeline *)ttl);
}

DP_Frame *DP_transient_timeline_frame_at(DP_TransientTimeline *ttl, int index)
{
    return DP_timeline_frame_at_noinc((DP_Timeline *)ttl, index);
}

void DP_transient_timeline_insert_transient_noinc(DP_TransientTimeline *ttl,
                                                  DP_TransientFrame *tf,
                                                  int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(!ttl->elements[ttl->count - 1].frame);
    DP_ASSERT(tf);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    memmove(&ttl->elements[index + 1], &ttl->elements[index],
            sizeof(*ttl->elements) * DP_int_to_size(ttl->count - index - 1));
    ttl->elements[index].transient_frame = tf;
}

void DP_transient_timeline_replace_transient_noinc(DP_TransientTimeline *ttl,
                                                   DP_TransientFrame *tf,
                                                   int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(ttl->elements[index].frame);
    DP_ASSERT(tf);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    DP_frame_decref(ttl->elements[index].frame);
    ttl->elements[index].transient_frame = tf;
}

void DP_transient_timeline_delete_at(DP_TransientTimeline *ttl, int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    DP_frame_decref(ttl->elements[index].frame);
    int new_count = --ttl->count;
    memmove(&ttl->elements[index], &ttl->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(ttl->elements[0]));
}
