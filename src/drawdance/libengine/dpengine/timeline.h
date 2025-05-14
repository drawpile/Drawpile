// SPDX-License-Identifier: MIT
#ifndef DPENGINE_TIMELINE_H
#define DPENGINE_TIMELINE_H
#include <dpcommon/common.h>

typedef struct DP_Track DP_Track;
typedef struct DP_Timeline DP_Timeline;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientTrack DP_TransientTrack;
typedef struct DP_TransientTimeline DP_TransientTimeline;
#else
typedef struct DP_Track DP_TransientTrack;
typedef struct DP_Timeline DP_TransientTimeline;
#endif


DP_Timeline *DP_timeline_new(void);

DP_Timeline *DP_timeline_incref(DP_Timeline *tl);

DP_Timeline *DP_timeline_incref_nullable(DP_Timeline *tl_or_null);

void DP_timeline_decref(DP_Timeline *tl);

void DP_timeline_decref_nullable(DP_Timeline *tl_or_null);

int DP_timeline_refcount(DP_Timeline *tl);

bool DP_timeline_transient(DP_Timeline *tl);

int DP_timeline_count(DP_Timeline *tl);

DP_Track *DP_timeline_at_noinc(DP_Timeline *tl, int index);

int DP_timeline_index_by_id(DP_Timeline *tl, int track_id);

bool DP_timeline_same_frame(DP_Timeline *tl, int frame_index_a,
                            int frame_index_b);


DP_TransientTimeline *DP_transient_timeline_new(DP_Timeline *tl, int reserve);

DP_TransientTimeline *DP_transient_timeline_new_init(int reserve);

DP_TransientTimeline *DP_transient_timeline_reserve(DP_TransientTimeline *ttl,
                                                    int reserve);

DP_TransientTimeline *DP_transient_timeline_incref(DP_TransientTimeline *ttl);

void DP_transient_timeline_decref(DP_TransientTimeline *ttl);

int DP_transient_timeline_refcount(DP_TransientTimeline *ttl);

DP_Timeline *DP_transient_timeline_persist(DP_TransientTimeline *ttl);

DP_Track *DP_transient_timeline_at_noinc(DP_TransientTimeline *ttl, int index);

DP_TransientTrack *
DP_transient_timeline_transient_at_noinc(DP_TransientTimeline *ttl, int index,
                                         int reserve);

int DP_transient_timeline_index_by_id(DP_TransientTimeline *ttl, int track_id);

void DP_transient_timeline_set_noinc(DP_TransientTimeline *ttl, DP_Track *t,
                                     int index);

void DP_transient_timeline_set_inc(DP_TransientTimeline *ttl, DP_Track *t,
                                   int index);

void DP_transient_timeline_set_transient_noinc(DP_TransientTimeline *ttl,
                                               DP_TransientTrack *tt,
                                               int index);

void DP_transient_timeline_insert_transient_noinc(DP_TransientTimeline *ttl,
                                                  DP_TransientTrack *tt,
                                                  int index);

void DP_transient_timeline_delete_at(DP_TransientTimeline *ttl, int index);

void DP_transient_timeline_clamp(DP_TransientTimeline *ttl, int count);


#endif
