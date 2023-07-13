// SPDX-License-Identifier: MIT
#ifndef DPENGINE_TRACK_H
#define DPENGINE_TRACK_H
#include <dpcommon/common.h>

typedef struct DP_KeyFrame DP_KeyFrame;
typedef struct DP_Track DP_Track;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientKeyFrame DP_TransientKeyFrame;
typedef struct DP_TransientTrack DP_TransientTrack;
#else
typedef struct DP_KeyFrame DP_TransientKeyFrame;
typedef struct DP_Track DP_TransientTrack;
#endif


DP_Track *DP_track_incref(DP_Track *t);

DP_Track *DP_track_incref_nullable(DP_Track *t_or_null);

void DP_track_decref(DP_Track *t);

void DP_track_decref_nullable(DP_Track *t_or_null);

int DP_track_refcount(DP_Track *t);

bool DP_track_transient(DP_Track *t);

int DP_track_id(DP_Track *t);

const char *DP_track_title(DP_Track *t, size_t *out_length);

bool DP_track_hidden(DP_Track *t);

bool DP_track_onion_skin(DP_Track *t);

int DP_track_key_frame_count(DP_Track *t);

int DP_track_frame_index_at_noinc(DP_Track *t, int index);

DP_KeyFrame *DP_track_key_frame_at_noinc(DP_Track *t, int index);

DP_KeyFrame *DP_track_key_frame_at_inc(DP_Track *t, int index);

int DP_track_key_frame_search_at(DP_Track *t, int frame_index);

int DP_track_key_frame_search_at_or_before(DP_Track *t, int frame_index);

int DP_track_key_frame_search_at_or_after(DP_Track *t, int frame_index,
                                          bool *out_exact);

bool DP_track_same_frame(DP_Track *t, int frame_index_a, int frame_index_b);


DP_TransientTrack *DP_transient_track_new_init(int reserve);

DP_TransientTrack *DP_transient_track_new(DP_Track *t, int reserve);

DP_TransientTrack *DP_transient_track_reserve(DP_TransientTrack *tt,
                                              int reserve);

DP_TransientTrack *DP_transient_track_incref(DP_TransientTrack *tt);

void DP_transient_track_decref(DP_TransientTrack *tt);

int DP_transient_track_refcount(DP_TransientTrack *tt);

DP_Track *DP_transient_track_persist(DP_TransientTrack *tt);

DP_KeyFrame *DP_transient_track_key_frame_at_noinc(DP_TransientTrack *tt,
                                                   int index);

int DP_transient_track_key_frame_search_at(DP_TransientTrack *tt,
                                           int frame_index);

void DP_transient_track_id_set(DP_TransientTrack *tt, int id);

void DP_transient_track_title_set(DP_TransientTrack *tt, const char *title,
                                  size_t length);

void DP_transient_track_hidden_set(DP_TransientTrack *tt, bool hidden);

void DP_transient_track_onion_skin_set(DP_TransientTrack *tt, bool onion_skin);

void DP_transient_track_truncate(DP_TransientTrack *tt, int to_count);

DP_TransientKeyFrame *
DP_transient_track_transient_at_noinc(DP_TransientTrack *tt, int index);

void DP_transient_track_set_transient_noinc(DP_TransientTrack *tt,
                                            int frame_index,
                                            DP_TransientKeyFrame *tkf,
                                            int index);

void DP_transient_track_replace_noinc(DP_TransientTrack *tt, int frame_index,
                                      DP_KeyFrame *kf, int index);

void DP_transient_track_replace_transient_noinc(DP_TransientTrack *tt,
                                                int frame_index,
                                                DP_TransientKeyFrame *tkf,
                                                int index);

void DP_transient_track_insert_noinc(DP_TransientTrack *tt, int frame_index,
                                     DP_KeyFrame *kf, int index);

void DP_transient_track_delete_at(DP_TransientTrack *tt, int index);


#endif
