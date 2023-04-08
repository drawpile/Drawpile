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
#include "annotation_list.h"
#include "annotation.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_AnnotationList {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    struct {
        DP_Annotation *annotation;
    } elements[];
};

struct DP_TransientAnnotationList {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Annotation *annotation;
        DP_TransientAnnotation *transient_annotation;
    } elements[];
};

#else

struct DP_AnnotationList {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_Annotation *annotation;
        DP_TransientAnnotation *transient_annotation;
    } elements[];
};

#endif


static size_t annotation_list_size(int count)
{
    DP_ASSERT(count >= 0);
    return DP_FLEX_SIZEOF(DP_TransientAnnotationList, elements,
                          DP_int_to_size(count));
}

static void *alloc_annotation_list(bool transient, int count)
{
    DP_TransientAnnotationList *tal = DP_malloc(annotation_list_size(count));
    DP_atomic_set(&tal->refcount, 1);
    tal->transient = transient;
    tal->count = count;
    return tal;
}

DP_AnnotationList *DP_annotation_list_new(void)
{
    return alloc_annotation_list(false, 0);
}

DP_AnnotationList *DP_annotation_list_incref(DP_AnnotationList *al)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    DP_atomic_inc(&al->refcount);
    return al;
}

DP_AnnotationList *
DP_annotation_list_incref_nullable(DP_AnnotationList *al_or_null)
{
    return al_or_null ? DP_annotation_list_incref(al_or_null) : NULL;
}

void DP_annotation_list_decref(DP_AnnotationList *al)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    if (DP_atomic_dec(&al->refcount)) {
        int count = al->count;
        for (int i = 0; i < count; ++i) {
            DP_annotation_decref_nullable(al->elements[i].annotation);
        }
        DP_free(al);
    }
}

void DP_annotation_list_decref_nullable(DP_AnnotationList *al_or_null)
{
    if (al_or_null) {
        DP_annotation_list_decref(al_or_null);
    }
}

int DP_annotation_list_refcount(DP_AnnotationList *al)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    return DP_atomic_get(&al->refcount);
}

bool DP_annotation_list_transient(DP_AnnotationList *al)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    return al->transient;
}

int DP_annotation_list_count(DP_AnnotationList *al)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    return al->count;
}

DP_Annotation *DP_annotation_list_at_noinc(DP_AnnotationList *al, int index)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < al->count);
    return al->elements[index].annotation;
}

int DP_annotation_list_index_by_id(DP_AnnotationList *al, int annotation_id)
{
    DP_ASSERT(al);
    DP_ASSERT(DP_atomic_get(&al->refcount) > 0);
    int count = al->count;
    for (int i = 0; i < count; ++i) {
        DP_Annotation *a = al->elements[i].annotation;
        // Null elements may be allocated in reserve.
        if (a && DP_annotation_id(a) == annotation_id) {
            return i;
        }
    }
    return -1;
}


DP_TransientAnnotationList *
DP_transient_annotation_list_new(DP_AnnotationList *al, int reserve)
{
    DP_ASSERT(al);
    DP_ASSERT(reserve >= 0);
    int old_count = al->count;
    int new_count = old_count + reserve;
    DP_TransientAnnotationList *tal = alloc_annotation_list(true, new_count);
    for (int i = 0; i < old_count; ++i) {
        tal->elements[i].annotation =
            DP_annotation_incref(al->elements[i].annotation);
    }
    for (int i = old_count; i < new_count; ++i) {
        tal->elements[i].annotation = NULL;
    }
    return tal;
}

DP_TransientAnnotationList *
DP_transient_annotation_list_reserve(DP_TransientAnnotationList *tal,
                                     int reserve)
{
    DP_ASSERT(tal);
    DP_ASSERT(DP_atomic_get(&tal->refcount) > 0);
    DP_ASSERT(tal->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in annotation list", reserve);
    if (reserve > 0) {
        int old_count = tal->count;
        int new_count = old_count + reserve;
        tal = DP_realloc(tal, annotation_list_size(new_count));
        tal->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tal->elements[i].annotation = NULL;
        }
    }
    return tal;
}

DP_TransientAnnotationList *
DP_transient_annotation_list_incref(DP_TransientAnnotationList *tal)
{
    return (DP_TransientAnnotationList *)DP_annotation_list_incref(
        (DP_AnnotationList *)tal);
}

void DP_transient_annotation_list_decref(DP_TransientAnnotationList *tal)
{
    DP_annotation_list_decref((DP_AnnotationList *)tal);
}

int DP_transient_annotation_list_refcount(DP_TransientAnnotationList *tal)
{
    return DP_annotation_list_refcount((DP_AnnotationList *)tal);
}

DP_AnnotationList *
DP_transient_annotation_list_persist(DP_TransientAnnotationList *tal)
{
    DP_ASSERT(tal);
    DP_ASSERT(DP_atomic_get(&tal->refcount) > 0);
    DP_ASSERT(tal->transient);
    int count = tal->count;
    for (int i = 0; i < count; ++i) {
        if (DP_annotation_transient(tal->elements[i].annotation)) {
            DP_transient_annotation_persist(
                tal->elements[i].transient_annotation);
        }
    }
    tal->transient = false;
    return (DP_AnnotationList *)tal;
}

int DP_transient_annotation_list_count(DP_TransientAnnotationList *tal)
{
    return DP_annotation_list_count((DP_AnnotationList *)tal);
}

DP_Annotation *
DP_transient_annotation_list_at_noinc(DP_TransientAnnotationList *tal,
                                      int index)
{
    return DP_annotation_list_at_noinc((DP_AnnotationList *)tal, index);
}

DP_TransientAnnotation *
DP_transient_annotation_list_transient_at_noinc(DP_TransientAnnotationList *tal,
                                                int index)
{
    DP_ASSERT(tal);
    DP_ASSERT(DP_atomic_get(&tal->refcount) > 0);
    DP_ASSERT(tal->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tal->count);
    DP_Annotation *a = tal->elements[index].annotation;
    if (DP_annotation_transient(a)) {
        return tal->elements[index].transient_annotation;
    }
    else {
        DP_TransientAnnotation *ta = DP_transient_annotation_new(a);
        DP_annotation_decref(a);
        tal->elements[index].transient_annotation = ta;
        return ta;
    }
}

int DP_transient_annotation_list_index_by_id(DP_TransientAnnotationList *tal,
                                             int annotation_id)
{
    return DP_annotation_list_index_by_id((DP_AnnotationList *)tal,
                                          annotation_id);
}

void DP_transient_annotation_list_insert_noinc(DP_TransientAnnotationList *tal,
                                               DP_Annotation *a, int index)
{
    DP_ASSERT(tal);
    DP_ASSERT(DP_atomic_get(&tal->refcount) > 0);
    DP_ASSERT(tal->transient);
    DP_ASSERT(!tal->elements[tal->count - 1].annotation);
    DP_ASSERT(a);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tal->count);
    memmove(&tal->elements[index + 1], &tal->elements[index],
            sizeof(*tal->elements) * DP_int_to_size(tal->count - index - 1));
    tal->elements[index].annotation = a;
}

void DP_transient_annotation_list_delete_at(DP_TransientAnnotationList *tal,
                                            int index)
{
    DP_ASSERT(tal);
    DP_ASSERT(DP_atomic_get(&tal->refcount) > 0);
    DP_ASSERT(tal->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tal->count);
    DP_annotation_decref(tal->elements[index].annotation);
    int new_count = --tal->count;
    memmove(&tal->elements[index], &tal->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tal->elements[0]));
}
