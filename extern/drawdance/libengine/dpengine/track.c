// SPDX-License-Identifier: MIT
#include "track.h"
#include "key_frame.h"
#include "text.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


typedef struct DP_TrackEntry {
    int frame_index;
    union {
        DP_KeyFrame *key_frame;
        DP_TransientKeyFrame *transient_key_frame;
    };
} DP_TrackEntry;

#ifdef DP_NO_STRICT_ALIASING

struct DP_Track {
    DP_Atomic refcount;
    const bool transient;
    const bool hidden;
    const bool onion_skin;
    const int id;
    DP_Text *const title;
    const int count;
    DP_TrackEntry elements[];
};

struct DP_TransientTrack {
    DP_Atomic refcount;
    bool transient;
    bool hidden;
    bool onion_skin;
    int id;
    DP_Text *title;
    int count;
    DP_TrackEntry elements[];
};

#else

struct DP_Track {
    DP_Atomic refcount;
    bool transient;
    bool hidden;
    bool onion_skin;
    int id;
    DP_Text *title;
    int count;
    DP_TrackEntry elements[];
};

#endif


static size_t track_size(int count)
{
    return DP_FLEX_SIZEOF(DP_Track, elements, DP_int_to_size(count));
}

static void *allocate_track(bool transient, int count)
{
    DP_TransientTrack *tt = DP_malloc(track_size(count));
    DP_atomic_set(&tt->refcount, 1);
    tt->transient = transient;
    tt->count = count;
    return tt;
}


DP_Track *DP_track_incref(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    DP_atomic_inc(&t->refcount);
    return t;
}

DP_Track *DP_track_incref_nullable(DP_Track *t_or_null)
{
    return t_or_null ? DP_track_incref(t_or_null) : NULL;
}

void DP_track_decref(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    if (DP_atomic_dec(&t->refcount)) {
        int count = t->count;
        for (int i = 0; i < count; ++i) {
            DP_key_frame_decref_nullable(t->elements[i].key_frame);
        }
        DP_text_decref_nullable(t->title);
        DP_free(t);
    }
}

void DP_track_decref_nullable(DP_Track *t_or_null)
{
    if (t_or_null) {
        DP_track_decref(t_or_null);
    }
}

int DP_track_refcount(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return DP_atomic_get(&t->refcount);
}

bool DP_track_transient(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return t->transient;
}

int DP_track_id(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return t->id;
}

const char *DP_track_title(DP_Track *t, size_t *out_length)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return DP_text_string(t->title, out_length);
}

bool DP_track_hidden(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return t->hidden;
}

bool DP_track_onion_skin(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return t->onion_skin;
}

int DP_track_key_frame_count(DP_Track *t)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    return t->count;
}

int DP_track_frame_index_at_noinc(DP_Track *t, int index)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < t->count);
    return t->elements[index].frame_index;
}

DP_KeyFrame *DP_track_key_frame_at_noinc(DP_Track *t, int index)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < t->count);
    return t->elements[index].key_frame;
}

DP_KeyFrame *DP_track_key_frame_at_inc(DP_Track *t, int index)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < t->count);
    return DP_key_frame_incref(DP_track_key_frame_at_noinc(t, index));
}

int DP_track_key_frame_search_at(DP_Track *t, int frame_index)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    int count = t->count;
    for (int i = 0; i < count; ++i) {
        if (frame_index == t->elements[i].frame_index) {
            return i;
        }
    }
    return -1;
}

int DP_track_key_frame_search_at_or_before(DP_Track *t, int frame_index)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    int count = t->count;
    int found = -1;
    for (int i = 0; i < count; ++i) {
        int current = t->elements[i].frame_index;
        if (current <= frame_index) {
            found = i;
        }
        else {
            break;
        }
    }
    return found;
}

int DP_track_key_frame_search_at_or_after(DP_Track *t, int frame_index,
                                          bool *out_exact)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    int count = t->count;
    for (int i = 0; i < count; ++i) {
        int current = t->elements[i].frame_index;
        if (current >= frame_index) {
            if (out_exact) {
                *out_exact = current == frame_index;
            }
            return i;
        }
    }
    if (out_exact) {
        *out_exact = false;
    }
    return -1;
}

bool DP_track_same_frame(DP_Track *t, int frame_index_a, int frame_index_b)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    if (frame_index_a != frame_index_b) {
        int kf_index_a =
            DP_track_key_frame_search_at_or_before(t, frame_index_a);
        int kf_index_b =
            DP_track_key_frame_search_at_or_before(t, frame_index_b);
        if (kf_index_a != kf_index_b) {
            DP_KeyFrame *kf_a =
                kf_index_a == -1 ? NULL : t->elements[kf_index_a].key_frame;
            DP_KeyFrame *kf_b =
                kf_index_b == -1 ? NULL : t->elements[kf_index_b].key_frame;
            if (!DP_key_frame_same_frame(kf_a, kf_b)) {
                return false;
            }
        }
    }
    return true;
}


DP_TransientTrack *DP_transient_track_new_init(int reserve)
{
    DP_TransientTrack *tt = allocate_track(true, reserve);
    tt->hidden = false;
    tt->onion_skin = false;
    tt->id = 0;
    tt->title = NULL;
    for (int i = 0; i < reserve; ++i) {
        tt->elements[i] = (DP_TrackEntry){-1, {NULL}};
    }
    return tt;
}

DP_TransientTrack *DP_transient_track_new(DP_Track *t, int reserve)
{
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    DP_ASSERT(!t->transient);
    DP_ASSERT(reserve >= 0);
    int count = t->count;
    DP_TransientTrack *tt = allocate_track(true, count + reserve);
    tt->hidden = t->hidden;
    tt->onion_skin = t->onion_skin;
    tt->id = t->id;
    tt->title = DP_text_incref_nullable(t->title);
    for (int i = 0; i < count; ++i) {
        DP_TrackEntry *te = &t->elements[i];
        tt->elements[i] = (DP_TrackEntry){te->frame_index,
                                          {DP_key_frame_incref(te->key_frame)}};
    }
    for (int i = 0; i < reserve; ++i) {
        tt->elements[count + i] = (DP_TrackEntry){-1, {NULL}};
    }
    return tt;
}

DP_TransientTrack *DP_transient_track_reserve(DP_TransientTrack *tt,
                                              int reserve)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in track", reserve);
    if (reserve > 0) {
        int old_count = tt->count;
        int new_count = old_count + reserve;
        tt = DP_realloc(tt, track_size(new_count));
        tt->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tt->elements[i] = (DP_TrackEntry){-1, {NULL}};
        }
    }
    return tt;
}

DP_TransientTrack *DP_transient_track_incref(DP_TransientTrack *tt)
{
    return (DP_TransientTrack *)DP_track_incref((DP_Track *)tt);
}

void DP_transient_track_decref(DP_TransientTrack *tt)
{
    DP_track_decref((DP_Track *)tt);
}

int DP_transient_track_refcount(DP_TransientTrack *tt)
{
    return DP_track_refcount((DP_Track *)tt);
}

DP_Track *DP_transient_track_persist(DP_TransientTrack *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->transient = false;
    int count = tt->count;
    for (int i = 0; i < count; ++i) {
        DP_ASSERT(i == 0
                  || tt->elements[i].frame_index
                         > tt->elements[i - 1].frame_index);
        DP_ASSERT(tt->elements[i].key_frame);
        if (DP_key_frame_transient(tt->elements[i].key_frame)) {
            DP_transient_key_frame_persist(tt->elements[i].transient_key_frame);
        }
    }
    return (DP_Track *)tt;
}

DP_KeyFrame *DP_transient_track_key_frame_at_noinc(DP_TransientTrack *tt,
                                                   int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return DP_track_key_frame_at_noinc((DP_Track *)tt, index);
}

int DP_transient_track_key_frame_search_at(DP_TransientTrack *tt,
                                           int frame_index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return DP_track_key_frame_search_at((DP_Track *)tt, frame_index);
}

void DP_transient_track_id_set(DP_TransientTrack *tt, int id)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->id = id;
}

void DP_transient_track_title_set(DP_TransientTrack *tt, const char *title,
                                  size_t length)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_text_decref_nullable(tt->title);
    tt->title = DP_text_new(title, length);
}

void DP_transient_track_hidden_set(DP_TransientTrack *tt, bool hidden)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->hidden = hidden;
}

void DP_transient_track_onion_skin_set(DP_TransientTrack *tt, bool onion_skin)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->onion_skin = onion_skin;
}

void DP_transient_track_truncate(DP_TransientTrack *tt, int to_count)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(to_count <= tt->count);
    int count = tt->count;
    for (int i = to_count; i < count; ++i) {
        DP_key_frame_decref_nullable(tt->elements[i].key_frame);
    }
    tt->count = to_count;
}

DP_TransientKeyFrame *
DP_transient_track_transient_at_noinc(DP_TransientTrack *tt, int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tt->count);
    DP_KeyFrame *kf = tt->elements[index].key_frame;
    DP_TransientKeyFrame *tkf;
    if (DP_key_frame_transient(kf)) {
        tkf = tt->elements[index].transient_key_frame;
    }
    else {
        tkf = DP_transient_key_frame_new(kf);
        tt->elements[index].transient_key_frame = tkf;
        DP_key_frame_decref(kf);
    }
    return tkf;
}

void DP_transient_track_set_transient_noinc(DP_TransientTrack *tt,
                                            int frame_index,
                                            DP_TransientKeyFrame *tkf,
                                            int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tt->count);
    DP_ASSERT(!tt->elements[index].key_frame);
    tt->elements[index] =
        (DP_TrackEntry){frame_index, {.transient_key_frame = tkf}};
}

void DP_transient_track_replace_noinc(DP_TransientTrack *tt, int frame_index,
                                      DP_KeyFrame *kf, int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tt->count);
    DP_ASSERT(tt->elements[index].key_frame);
    DP_key_frame_decref(tt->elements[index].key_frame);
    tt->elements[index] = (DP_TrackEntry){frame_index, {.key_frame = kf}};
}

void DP_transient_track_replace_transient_noinc(DP_TransientTrack *tt,
                                                int frame_index,
                                                DP_TransientKeyFrame *tkf,
                                                int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tt->count);
    DP_ASSERT(tt->elements[index].key_frame);
    DP_key_frame_decref(tt->elements[index].key_frame);
    tt->elements[index] =
        (DP_TrackEntry){frame_index, {.transient_key_frame = tkf}};
}

void DP_transient_track_insert_noinc(DP_TransientTrack *tt, int frame_index,
                                     DP_KeyFrame *kf, int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(!tt->elements[tt->count - 1].key_frame);
    DP_ASSERT(tt);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tt->count);
    memmove(&tt->elements[index + 1], &tt->elements[index],
            sizeof(*tt->elements) * DP_int_to_size(tt->count - index - 1));
    tt->elements[index] = (DP_TrackEntry){frame_index, {.key_frame = kf}};
}

void DP_transient_track_delete_at(DP_TransientTrack *tt, int index)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(tt);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tt->count);
    DP_ASSERT(tt->elements[index].key_frame);
    int new_count = --tt->count;
    DP_key_frame_decref(tt->elements[index].key_frame);
    memmove(&tt->elements[index], &tt->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tt->elements[0]));
}
