// SPDX-License-Identifier: MIT
#include "timeline.h"
#include "track.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_Timeline {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    struct {
        DP_Track *const track;
    } elements[];
};

struct DP_TransientTimeline {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Track *track;
        DP_TransientTrack *transient_track;
    } elements[];
};

#else

struct DP_Timeline {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Track *track;
        DP_TransientTrack *transient_track;
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
            DP_track_decref_nullable(tl->elements[i].track);
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
    return DP_atomic_get(&tl->refcount);
}

bool DP_timeline_transient(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    return tl->transient;
}

int DP_timeline_count(DP_Timeline *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    return tl->count;
}

DP_Track *DP_timeline_at_noinc(DP_Timeline *tl, int index)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tl->count);
    return tl->elements[index].track;
}

int DP_timeline_index_by_id(DP_Timeline *tl, int track_id)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    int count = tl->count;
    for (int i = 0; i < count; ++i) {
        DP_Track *t = tl->elements[i].track;
        if (t && DP_track_id(t) == track_id) {
            return i;
        }
    }
    return -1;
}

bool DP_timeline_same_frame(DP_Timeline *tl, int frame_index_a,
                            int frame_index_b)
{
    DP_ASSERT(tl);
    DP_ASSERT(DP_atomic_get(&tl->refcount) > 0);
    if (frame_index_a != frame_index_b) {
        int track_count = tl->count;
        for (int i = 0; i < track_count; ++i) {
            DP_Track *t = tl->elements[i].track;
            if (!DP_track_same_frame(t, frame_index_a, frame_index_b)) {
                return false;
            }
        }
    }
    return true;
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
        ttl->elements[i].track = DP_track_incref(tl->elements[i].track);
    }
    for (int i = 0; i < reserve; ++i) {
        ttl->elements[count + i].track = NULL;
    }
    return ttl;
}

DP_TransientTimeline *DP_transient_timeline_new_init(int reserve)
{
    DP_ASSERT(reserve >= 0);
    DP_TransientTimeline *ttl = allocate_timeline(true, reserve);
    for (int i = 0; i < reserve; ++i) {
        ttl->elements[i].track = NULL;
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
            ttl->elements[i].track = NULL;
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
        DP_ASSERT(ttl->elements[i].track);
        if (DP_track_transient(ttl->elements[i].track)) {
            DP_transient_track_persist(ttl->elements[i].transient_track);
        }
    }
    return (DP_Timeline *)ttl;
}

DP_Track *DP_transient_timeline_at_noinc(DP_TransientTimeline *ttl, int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    return DP_timeline_at_noinc((DP_Timeline *)ttl, index);
}

DP_TransientTrack *
DP_transient_timeline_transient_at_noinc(DP_TransientTimeline *ttl, int index,
                                         int reserve)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    DP_Track *t = ttl->elements[index].track;
    DP_TransientTrack *tt;
    if (DP_track_transient(t)) {
        tt = ttl->elements[index].transient_track;
        DP_transient_track_reserve(tt, reserve);
    }
    else {
        tt = DP_transient_track_new(t, reserve);
        ttl->elements[index].transient_track = tt;
        DP_track_decref(t);
    }
    return tt;
}

int DP_transient_timeline_index_by_id(DP_TransientTimeline *ttl, int track_id)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    return DP_timeline_index_by_id((DP_Timeline *)ttl, track_id);
}

void DP_transient_timeline_set_noinc(DP_TransientTimeline *ttl, DP_Track *t,
                                     int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(t);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    DP_ASSERT(!ttl->elements[index].track);
    ttl->elements[index].track = t;
}

void DP_transient_timeline_set_inc(DP_TransientTimeline *ttl, DP_Track *t,
                                   int index)
{
    DP_transient_timeline_set_noinc(ttl, DP_track_incref(t), index);
}

void DP_transient_timeline_set_transient_noinc(DP_TransientTimeline *ttl,
                                               DP_TransientTrack *tt, int index)
{
    DP_transient_timeline_set_noinc(ttl, (DP_Track *)tt, index);
}

void DP_transient_timeline_insert_transient_noinc(DP_TransientTimeline *ttl,
                                                  DP_TransientTrack *tt,
                                                  int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(!ttl->elements[ttl->count - 1].track);
    DP_ASSERT(tt);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    memmove(&ttl->elements[index + 1], &ttl->elements[index],
            sizeof(*ttl->elements) * DP_int_to_size(ttl->count - index - 1));
    ttl->elements[index].transient_track = tt;
}

void DP_transient_timeline_delete_at(DP_TransientTimeline *ttl, int index)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ttl->count);
    DP_ASSERT(ttl->elements[index].track);
    int new_count = --ttl->count;
    DP_track_decref(ttl->elements[index].track);
    memmove(&ttl->elements[index], &ttl->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(ttl->elements[0]));
}

void DP_transient_timeline_clamp(DP_TransientTimeline *ttl, int count)
{
    DP_ASSERT(ttl);
    DP_ASSERT(DP_atomic_get(&ttl->refcount) > 0);
    DP_ASSERT(ttl->transient);
    DP_ASSERT(count >= 0);
    DP_ASSERT(count <= ttl->count);
    int old_count = ttl->count;
    for (int i = count; i < old_count; ++i) {
        DP_track_decref_nullable(ttl->elements[i].track);
    }
    ttl->count = count;
}
