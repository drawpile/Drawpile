/*
 * Copyright (C) 2022 askmeaboutloom
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
#ifndef DPENGINE_TIMELINE_H
#define DPENGINE_TIMELINE_H
#include <dpcommon/common.h>

typedef struct DP_Frame DP_Frame;
typedef struct DP_Timeline DP_Timeline;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientFrame DP_TransientFrame;
typedef struct DP_TransientTimeline DP_TransientTimeline;
#else
typedef struct DP_Frame DP_TransientFrame;
typedef struct DP_Timeline DP_TransientTimeline;
#endif


DP_Timeline *DP_timeline_new(void);

DP_Timeline *DP_timeline_incref(DP_Timeline *tl);

DP_Timeline *DP_timeline_incref_nullable(DP_Timeline *tl_or_null);

void DP_timeline_decref(DP_Timeline *tl);

void DP_timeline_decref_nullable(DP_Timeline *tl_or_null);

int DP_timeline_refcount(DP_Timeline *tl);

bool DP_timeline_transient(DP_Timeline *tl);

int DP_timeline_frame_count(DP_Timeline *tl);

DP_Frame *DP_timeline_frame_at_noinc(DP_Timeline *tl, int index);


DP_TransientTimeline *DP_transient_timeline_new(DP_Timeline *tl, int reserve);

DP_TransientTimeline *DP_transient_timeline_reserve(DP_TransientTimeline *ttl,
                                                    int reserve);

DP_TransientTimeline *DP_transient_timeline_incref(DP_TransientTimeline *ttl);

void DP_transient_timeline_decref(DP_TransientTimeline *ttl);

int DP_transient_timeline_refcount(DP_TransientTimeline *ttl);

DP_Timeline *DP_transient_timeline_persist(DP_TransientTimeline *ttl);

int DP_transient_timeline_frame_count(DP_TransientTimeline *ttl);

DP_Frame *DP_transient_timeline_frame_at(DP_TransientTimeline *ttl, int index);

void DP_transient_timeline_insert_transient_noinc(DP_TransientTimeline *ttl,
                                                  DP_TransientFrame *tf,
                                                  int index);

void DP_transient_timeline_replace_transient_noinc(DP_TransientTimeline *ttl,
                                                   DP_TransientFrame *tf,
                                                   int index);

void DP_transient_timeline_delete_at(DP_TransientTimeline *tl, int index);


#endif
