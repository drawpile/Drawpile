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
#ifndef DPENGINE_ANNOTATION_LIST_H
#define DPENGINE_ANNOTATION_LIST_H
#include <dpcommon/common.h>

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_Annotation DP_Annotation;
typedef struct DP_TransientAnnotation DP_TransientAnnotation;
typedef struct DP_AnnotationList DP_AnnotationList;
typedef struct DP_TransientAnnotationList DP_TransientAnnotationList;
#else
typedef struct DP_Annotation DP_Annotation;
typedef struct DP_Annotation DP_TransientAnnotation;
typedef struct DP_AnnotationList DP_AnnotationList;
typedef struct DP_AnnotationList DP_TransientAnnotationList;
#endif


DP_AnnotationList *DP_annotation_list_new(void);

DP_AnnotationList *DP_annotation_list_incref(DP_AnnotationList *al);

DP_AnnotationList *
DP_annotation_list_incref_nullable(DP_AnnotationList *al_or_null);

void DP_annotation_list_decref(DP_AnnotationList *al);

void DP_annotation_list_decref_nullable(DP_AnnotationList *al_or_null);

int DP_annotation_list_refcount(DP_AnnotationList *al);

bool DP_annotation_list_transient(DP_AnnotationList *al);

int DP_annotation_list_count(DP_AnnotationList *al);

DP_Annotation *DP_annotation_list_at_noinc(DP_AnnotationList *al, int index);

int DP_annotation_list_index_by_id(DP_AnnotationList *al, int annotation_id);


DP_TransientAnnotationList *
DP_transient_annotation_list_new(DP_AnnotationList *al, int reserve);

DP_TransientAnnotationList *
DP_transient_annotation_list_reserve(DP_TransientAnnotationList *tal,
                                     int reserve);

DP_TransientAnnotationList *
DP_transient_annotation_list_incref(DP_TransientAnnotationList *tal);

void DP_transient_annotation_list_decref(DP_TransientAnnotationList *tal);

int DP_transient_annotation_list_refcount(DP_TransientAnnotationList *tal);

DP_AnnotationList *
DP_transient_annotation_list_persist(DP_TransientAnnotationList *tal);

int DP_transient_annotation_list_count(DP_TransientAnnotationList *tal);

DP_Annotation *
DP_transient_annotation_list_at_noinc(DP_TransientAnnotationList *tal,
                                      int index);

DP_TransientAnnotation *
DP_transient_annotation_list_transient_at_noinc(DP_TransientAnnotationList *tal,
                                                int index);

int DP_transient_annotation_list_index_by_id(DP_TransientAnnotationList *tal,
                                             int annotation_id);

void DP_transient_annotation_list_insert_noinc(DP_TransientAnnotationList *tal,
                                               DP_Annotation *a, int index);

void DP_transient_annotation_list_delete_at(DP_TransientAnnotationList *tal,
                                            int index);


#endif
