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
#include "frame.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_Frame {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    struct {
        const int layer_id;
    } elements[];
};

struct DP_TransientFrame {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        int layer_id;
        int transient_layer_id;
    } elements[];
};

#else

struct DP_Frame {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        int layer_id;
        int transient_layer_id;
    } elements[];
};

#endif


DP_Frame *DP_frame_incref(DP_Frame *f)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    DP_atomic_inc(&f->refcount);
    return f;
}

DP_Frame *DP_frame_incref_nullable(DP_Frame *f_or_null)
{
    return f_or_null ? DP_frame_incref(f_or_null) : NULL;
}

void DP_frame_decref(DP_Frame *f)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    if (DP_atomic_dec(&f->refcount)) {
        DP_free(f);
    }
}

void DP_frame_decref_nullable(DP_Frame *f_or_null)
{
    if (f_or_null) {
        DP_frame_decref(f_or_null);
    }
}

int DP_frame_refcount(DP_Frame *f)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    return f->refcount;
}

bool DP_frame_transient(DP_Frame *f)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    return f->transient;
}

int DP_frame_layer_id_count(DP_Frame *f)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    return f->count;
}

int DP_frame_layer_id_at(DP_Frame *f, int index)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < f->count);
    return f->elements[index].layer_id;
}

bool DP_frame_layer_ids_contain(DP_Frame *f, int layer_id)
{
    DP_ASSERT(f);
    DP_ASSERT(DP_atomic_get(&f->refcount) > 0);
    int count = f->count;
    for (int i = 0; i < count; ++i) {
        if (f->elements[i].layer_id == layer_id) {
            return true;
        }
    }
    return false;
}


DP_TransientFrame *DP_transient_frame_new(DP_Frame *f, int reserve)
{
    DP_ASSERT(f);
    DP_ASSERT(reserve >= 0);
    int layer_id_count = f->count;
    DP_TransientFrame *tf =
        DP_transient_frame_new_init(layer_id_count + reserve);
    for (int i = 0; i < layer_id_count; ++i) {
        tf->elements[i].transient_layer_id = f->elements[i].layer_id;
    }
    for (int i = 0; i < reserve; ++i) {
        tf->elements[layer_id_count + i].transient_layer_id = 0;
    }
    return tf;
}

DP_TransientFrame *DP_transient_frame_new_init(int layer_id_count)
{
    DP_TransientFrame *tf = DP_malloc(DP_FLEX_SIZEOF(
        DP_TransientFrame, elements, DP_int_to_size(layer_id_count)));
    tf->refcount = DP_ATOMIC_INIT(1);
    tf->transient = true;
    tf->count = layer_id_count;
    return tf;
}

DP_TransientFrame *DP_transient_frame_incref(DP_TransientFrame *tf)
{
    return (DP_TransientFrame *)DP_frame_incref((DP_Frame *)tf);
}

void DP_transient_frame_decref(DP_TransientFrame *tf)
{
    DP_frame_decref((DP_Frame *)tf);
}

int DP_transient_frame_refcount(DP_TransientFrame *tf)
{
    return DP_frame_refcount((DP_Frame *)tf);
}

DP_Frame *DP_transient_frame_persist(DP_TransientFrame *tf)
{
    DP_ASSERT(tf);
    DP_ASSERT(DP_atomic_get(&tf->refcount) > 0);
    DP_ASSERT(tf->transient);
    tf->transient = false;
    return (DP_Frame *)tf;
}

int DP_transient_frame_layer_id_count(DP_TransientFrame *tf)
{
    return DP_frame_layer_id_count((DP_Frame *)tf);
}

int DP_transient_frame_layer_id_at(DP_TransientFrame *tf, int index)
{
    return DP_frame_layer_id_at((DP_Frame *)tf, index);
}

void DP_transient_frame_layer_id_set_at(DP_TransientFrame *tf, int layer_id,
                                        int index)
{
    DP_ASSERT(tf);
    DP_ASSERT(DP_atomic_get(&tf->refcount) > 0);
    DP_ASSERT(tf->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tf->count);
    tf->elements[index].transient_layer_id = layer_id;
}

void DP_transient_frame_layer_id_delete_at(DP_TransientFrame *tf, int index)
{
    DP_ASSERT(tf);
    DP_ASSERT(DP_atomic_get(&tf->refcount) > 0);
    DP_ASSERT(tf->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tf->count);
    int count = tf->count;
    tf->count = count - 1;
    for (int i = index + 1; i < count; ++i) {
        tf->elements[i - 1].transient_layer_id = tf->elements[i].layer_id;
    }
}
