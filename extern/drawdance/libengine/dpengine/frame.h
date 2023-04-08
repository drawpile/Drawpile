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
#ifndef DPENGINE_FRAME_H
#define DPENGINE_FRAME_H
#include <dpcommon/common.h>

typedef struct DP_Frame DP_Frame;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientFrame DP_TransientFrame;
#else
typedef struct DP_Frame DP_TransientFrame;
#endif


DP_Frame *DP_frame_incref(DP_Frame *f);

DP_Frame *DP_frame_incref_nullable(DP_Frame *f_or_null);

void DP_frame_decref(DP_Frame *f);

void DP_frame_decref_nullable(DP_Frame *f_or_null);

int DP_frame_refcount(DP_Frame *f);

bool DP_frame_transient(DP_Frame *f);

int DP_frame_layer_id_count(DP_Frame *f);

int DP_frame_layer_id_at(DP_Frame *f, int index);

bool DP_frame_layer_ids_contain(DP_Frame *f, int layer_id);


DP_TransientFrame *DP_transient_frame_new(DP_Frame *f, int reserve);

DP_TransientFrame *DP_transient_frame_new_init(int layer_id_count);

DP_TransientFrame *DP_transient_frame_incref(DP_TransientFrame *tf);

void DP_transient_frame_decref(DP_TransientFrame *tf);

int DP_transient_frame_refcount(DP_TransientFrame *tf);

DP_Frame *DP_transient_frame_persist(DP_TransientFrame *tf);

int DP_transient_frame_layer_id_count(DP_TransientFrame *tf);

int DP_transient_frame_layer_id_at(DP_TransientFrame *tf, int index);

void DP_transient_frame_layer_id_set_at(DP_TransientFrame *tf, int layer_id,
                                        int index);

void DP_transient_frame_layer_id_delete_at(DP_TransientFrame *tf, int index);


#endif
