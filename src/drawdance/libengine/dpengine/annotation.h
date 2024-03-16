/*
 * Copyright (C) 2022 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Genera Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Genera Public License for more details.
 *
 * You should have received a copy of the GNU Genera Public License
 * aong with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU Genera Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPENGINE_ANNOTATION_H
#define DPENGINE_ANNOTATION_H
#include <dpcommon/common.h>

#define DP_ANNOTATION_VALIGN_TOP    0
#define DP_ANNOTATION_VALIGN_CENTER 1
#define DP_ANNOTATION_VALIGN_BOTTOM 2

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_Annotation DP_Annotation;
typedef struct DP_TransientAnnotation DP_TransientAnnotation;
#else
typedef struct DP_Annotation DP_Annotation;
typedef struct DP_Annotation DP_TransientAnnotation;
#endif


DP_Annotation *DP_annotation_new(int id, int x, int y, int width, int height);

DP_Annotation *DP_annotation_incref(DP_Annotation *a);

DP_Annotation *DP_annotation_incref_nullable(DP_Annotation *a_or_null);

void DP_annotation_decref(DP_Annotation *a);

void DP_annotation_decref_nullable(DP_Annotation *a_or_null);

int DP_annotation_refcount(DP_Annotation *a);

bool DP_annotation_transient(DP_Annotation *a);

int DP_annotation_id(DP_Annotation *a);

int DP_annotation_x(DP_Annotation *a);

int DP_annotation_y(DP_Annotation *a);

int DP_annotation_width(DP_Annotation *a);

int DP_annotation_height(DP_Annotation *a);

uint32_t DP_annotation_background_color(DP_Annotation *a);

bool DP_annotation_protect(DP_Annotation *a);

bool DP_annotation_alias(DP_Annotation *a);

bool DP_annotation_rasterize(DP_Annotation *a);

int DP_annotation_valign(DP_Annotation *a);

const char *DP_annotation_text(DP_Annotation *a, size_t *out_length);

int DP_annotation_user_id(DP_Annotation *a);

bool DP_annotation_equal(DP_Annotation *a, DP_Annotation *b);


DP_TransientAnnotation *DP_transient_annotation_new(DP_Annotation *a);

DP_TransientAnnotation *DP_transient_annotation_new_init(int id, int x, int y,
                                                         int width, int height);

DP_TransientAnnotation *
DP_transient_annotation_incref(DP_TransientAnnotation *ta);

DP_TransientAnnotation *
DP_transient_annotation_incref_nullable(DP_TransientAnnotation *ta_or_null);

void DP_transient_annotation_decref(DP_TransientAnnotation *ta);

void DP_transient_annotation_decref_nullable(
    DP_TransientAnnotation *ta_or_null);

int DP_transient_annotation_refcount(DP_TransientAnnotation *ta);

DP_Annotation *DP_transient_annotation_persist(DP_TransientAnnotation *ta);

int DP_transient_annotation_id(DP_TransientAnnotation *ta);

int DP_transient_annotation_x(DP_TransientAnnotation *ta);

int DP_transient_annotation_y(DP_TransientAnnotation *ta);

int DP_transient_annotation_width(DP_TransientAnnotation *ta);

int DP_transient_annotation_height(DP_TransientAnnotation *ta);

uint32_t DP_transient_annotation_background_color(DP_TransientAnnotation *ta);

bool DP_transient_annotation_protect(DP_TransientAnnotation *ta);

bool DP_transient_annotation_alias(DP_TransientAnnotation *ta);

bool DP_transient_annotation_rasterize(DP_TransientAnnotation *ta);

int DP_transient_annotation_valign(DP_TransientAnnotation *ta);

const char *DP_transient_annotation_text(DP_TransientAnnotation *ta,
                                         size_t *out_length);

void DP_transient_annotation_id_set(DP_TransientAnnotation *ta, int id);

void DP_transient_annotation_x_set(DP_TransientAnnotation *ta, int x);

void DP_transient_annotation_y_set(DP_TransientAnnotation *ta, int y);

void DP_transient_annotation_width_set(DP_TransientAnnotation *ta, int width);

void DP_transient_annotation_height_set(DP_TransientAnnotation *ta, int height);

void DP_transient_annotation_background_color_set(DP_TransientAnnotation *ta,
                                                  uint32_t background_color);

void DP_transient_annotation_protect_set(DP_TransientAnnotation *ta,
                                         bool protect);

void DP_transient_annotation_alias_set(DP_TransientAnnotation *ta, bool alias);

void DP_transient_annotation_rasterize_set(DP_TransientAnnotation *ta,
                                           bool rasterize);

void DP_transient_annotation_valign_set(DP_TransientAnnotation *ta, int valign);

void DP_transient_annotation_text_set(DP_TransientAnnotation *ta,
                                      const char *text, size_t length);


#endif
