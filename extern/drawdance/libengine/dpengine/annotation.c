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
#include "annotation.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


typedef struct DP_AnnotationText {
    DP_Atomic refcount;
    size_t length;
    char text[];
} DP_AnnotationText;

#ifdef DP_NO_STRICT_ALIASING

struct DP_Annotation {
    DP_Atomic refcount;
    const bool transient;
    const int id;
    const int x;
    const int y;
    const int width;
    const int height;
    const uint32_t background_color;
    const bool protect;
    const int valign;
    DP_AnnotationText *const text;
};

struct DP_TransientAnnotation {
    DP_Atomic refcount;
    bool transient;
    int id;
    int x;
    int y;
    int width;
    int height;
    uint32_t background_color;
    bool protect;
    int valign;
    DP_AnnotationText *text;
};

#else

struct DP_Annotation {
    DP_Atomic refcount;
    bool transient;
    int id;
    int x;
    int y;
    int width;
    int height;
    uint32_t background_color;
    bool protect;
    int valign;
    DP_AnnotationText *text;
};

#endif


static DP_AnnotationText *annotation_text_new(const char *text, size_t length)
{
    DP_ASSERT(text);
    DP_ASSERT(length > 0);
    DP_AnnotationText *at =
        DP_malloc(DP_FLEX_SIZEOF(DP_AnnotationText, text, length + 1));
    DP_atomic_set(&at->refcount, 1);
    at->length = length;
    memcpy(at->text, text, length);
    at->text[length] = '\0';
    return at;
}

static DP_AnnotationText *annotation_text_incref(DP_AnnotationText *at)
{
    DP_ASSERT(at);
    DP_ASSERT(DP_atomic_get(&at->refcount) > 0);
    DP_atomic_inc(&at->refcount);
    return at;
}

static DP_AnnotationText *
annotation_text_incref_nullable(DP_AnnotationText *at_or_null)
{
    return at_or_null ? annotation_text_incref(at_or_null) : NULL;
}

static void annotation_text_decref(DP_AnnotationText *at)
{
    DP_ASSERT(at);
    DP_ASSERT(DP_atomic_get(&at->refcount) > 0);
    if (DP_atomic_dec(&at->refcount)) {
        DP_free(at);
    }
}

static void annotation_text_decref_nullable(DP_AnnotationText *at_or_null)
{
    if (at_or_null) {
        annotation_text_decref(at_or_null);
    }
}

static bool annotation_text_equal(DP_AnnotationText *a, DP_AnnotationText *b)
{
    return a == b
        || (a && b && a->length == b->length
            && memcmp(a->text, b->text, a->length) == 0);
}


static DP_TransientAnnotation *
allocate_annotation(bool transient, int id, int x, int y, int width, int height)
{
    DP_TransientAnnotation *ta = DP_malloc(sizeof(*ta));
    *ta = (DP_TransientAnnotation){
        DP_ATOMIC_INIT(1), transient, id, x, y, width, height, 0, 0, 0, NULL};
    return ta;
}

DP_Annotation *DP_annotation_new(int id, int x, int y, int width, int height)
{
    return (DP_Annotation *)allocate_annotation(false, id, x, y, width, height);
}

DP_Annotation *DP_annotation_incref(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    DP_atomic_inc(&a->refcount);
    return a;
}

DP_Annotation *DP_annotation_incref_nullable(DP_Annotation *a_or_null)
{
    return a_or_null ? DP_annotation_incref(a_or_null) : NULL;
}

void DP_annotation_decref(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    if (DP_atomic_dec(&a->refcount)) {
        annotation_text_decref_nullable(a->text);
        DP_free(a);
    }
}

void DP_annotation_decref_nullable(DP_Annotation *a_or_null)
{
    if (a_or_null) {
        DP_annotation_decref(a_or_null);
    }
}

int DP_annotation_refcount(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return DP_atomic_get(&a->refcount);
}

bool DP_annotation_transient(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->transient;
}

int DP_annotation_id(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->id;
}

int DP_annotation_x(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->x;
}

int DP_annotation_y(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->y;
}

int DP_annotation_width(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->width;
}

int DP_annotation_height(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->height;
}

uint32_t DP_annotation_background_color(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->background_color;
}

bool DP_annotation_protect(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->protect;
}

int DP_annotation_valign(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    return a->valign;
}

const char *DP_annotation_text(DP_Annotation *a, size_t *out_length)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);

    DP_AnnotationText *at = a->text;
    const char *text;
    size_t length;
    if (at) {
        text = at->text;
        length = at->length;
    }
    else {
        text = "";
        length = 0;
    }

    if (out_length) {
        *out_length = length;
    }
    return text;
}

int DP_annotation_user_id(DP_Annotation *a)
{
    return (DP_annotation_id(a) >> 8) & 0xff;
}

bool DP_annotation_equal(DP_Annotation *a, DP_Annotation *b)
{
    return a == b
        || (a && b && a->id == b->id && a->x == b->x && a->y == b->y
            && a->width == b->width && a->height == b->height
            && a->background_color == b->background_color
            && a->protect == b->protect && a->valign == b->valign
            && annotation_text_equal(a->text, b->text));
}


DP_TransientAnnotation *DP_transient_annotation_new(DP_Annotation *a)
{
    DP_ASSERT(a);
    DP_ASSERT(DP_atomic_get(&a->refcount) > 0);
    DP_TransientAnnotation *ta = DP_malloc(sizeof(*ta));
    *ta = (DP_TransientAnnotation){DP_ATOMIC_INIT(1),
                                   true,
                                   a->id,
                                   a->x,
                                   a->y,
                                   a->width,
                                   a->height,
                                   a->background_color,
                                   a->protect,
                                   a->valign,
                                   annotation_text_incref_nullable(a->text)};
    return ta;
}

DP_TransientAnnotation *DP_transient_annotation_new_init(int id, int x, int y,
                                                         int width, int height)
{
    return allocate_annotation(true, id, x, y, width, height);
}

DP_TransientAnnotation *
DP_transient_annotation_incref(DP_TransientAnnotation *ta)
{
    return (DP_TransientAnnotation *)DP_annotation_incref((DP_Annotation *)ta);
}

DP_TransientAnnotation *
DP_transient_annotation_incref_nullable(DP_TransientAnnotation *ta_or_null)
{
    return (DP_TransientAnnotation *)DP_annotation_incref_nullable(
        (DP_Annotation *)ta_or_null);
}

void DP_transient_annotation_decref(DP_TransientAnnotation *ta)
{
    DP_annotation_decref((DP_Annotation *)ta);
}

void DP_transient_annotation_decref_nullable(DP_TransientAnnotation *ta_or_null)
{
    DP_annotation_decref_nullable((DP_Annotation *)ta_or_null);
}

int DP_transient_annotation_refcount(DP_TransientAnnotation *ta)
{
    return DP_annotation_refcount((DP_Annotation *)ta);
}

DP_Annotation *DP_transient_annotation_persist(DP_TransientAnnotation *ta)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->transient = false;
    return (DP_Annotation *)ta;
}

int DP_transient_annotation_id(DP_TransientAnnotation *ta)
{
    return DP_annotation_id((DP_Annotation *)ta);
}

int DP_transient_annotation_x(DP_TransientAnnotation *ta)
{
    return DP_annotation_x((DP_Annotation *)ta);
}

int DP_transient_annotation_y(DP_TransientAnnotation *ta)
{
    return DP_annotation_y((DP_Annotation *)ta);
}

int DP_transient_annotation_width(DP_TransientAnnotation *ta)
{
    return DP_annotation_width((DP_Annotation *)ta);
}

int DP_transient_annotation_height(DP_TransientAnnotation *ta)
{
    return DP_annotation_height((DP_Annotation *)ta);
}

uint32_t DP_transient_annotation_background_color(DP_TransientAnnotation *ta)
{
    return DP_annotation_background_color((DP_Annotation *)ta);
}

bool DP_transient_annotation_protect(DP_TransientAnnotation *ta)
{
    return DP_annotation_protect((DP_Annotation *)ta);
}

int DP_transient_annotation_valign(DP_TransientAnnotation *ta)
{
    return DP_annotation_valign((DP_Annotation *)ta);
}

const char *DP_transient_annotation_text(DP_TransientAnnotation *ta,
                                         size_t *out_length)
{
    return DP_annotation_text((DP_Annotation *)ta, out_length);
}

void DP_transient_annotation_id_set(DP_TransientAnnotation *ta, int id)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->id = id;
}

void DP_transient_annotation_x_set(DP_TransientAnnotation *ta, int x)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->x = x;
}

void DP_transient_annotation_y_set(DP_TransientAnnotation *ta, int y)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->y = y;
}

void DP_transient_annotation_width_set(DP_TransientAnnotation *ta, int width)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->width = width;
}

void DP_transient_annotation_height_set(DP_TransientAnnotation *ta, int height)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->height = height;
}

void DP_transient_annotation_background_color_set(DP_TransientAnnotation *ta,
                                                  uint32_t background_color)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->background_color = background_color;
}

void DP_transient_annotation_protect_set(DP_TransientAnnotation *ta,
                                         bool protect)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    ta->protect = protect;
}

void DP_transient_annotation_valign_set(DP_TransientAnnotation *ta, int valign)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    switch (valign) {
    case DP_ANNOTATION_VALIGN_TOP:
    case DP_ANNOTATION_VALIGN_CENTER:
    case DP_ANNOTATION_VALIGN_BOTTOM:
        ta->valign = valign;
        break;
    default:
        DP_warn("Invalid valign value %d", valign);
        ta->valign = DP_ANNOTATION_VALIGN_TOP;
        break;
    }
}

void DP_transient_annotation_text_set(DP_TransientAnnotation *ta,
                                      const char *text, size_t length)
{
    DP_ASSERT(ta);
    DP_ASSERT(DP_atomic_get(&ta->refcount) > 0);
    DP_ASSERT(ta->transient);
    annotation_text_decref_nullable(ta->text);
    if (length > 0) {
        ta->text = annotation_text_new(text, length);
    }
    else {
        ta->text = NULL;
    }
}
