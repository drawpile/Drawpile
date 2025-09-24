// SPDX-License-Identifier: GPL-3.0-or-later
#include "camera.h"
#include "camera_key_frame.h"
#include "document_metadata.h"
#include "text.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>


#define INVALID_VIEWPORT                   \
    (DP_Rect)                              \
    {                                      \
        INT_MAX, INT_MAX, INT_MIN, INT_MIN \
    }

typedef struct DP_CameraEntry {
    int frame_index;
    union {
        DP_CameraKeyFrame *ckf;
        DP_TransientCameraKeyFrame *tckf;
    };
} DP_CameraEntry;

#ifdef DP_NO_STRICT_ALIASING

struct DP_Camera {
    DP_Atomic refcount;
    const bool transient;
    const bool hidden;
    const uint8_t flags;
    const uint8_t interpolation;
    const int id;
    const int framerate;
    const int framerate_fraction;
    const int range_first;
    const int range_last;
    const int output_width;
    const int output_height;
    const DP_Rect viewport;
    DP_Text *const title;
    const int count;
    DP_CameraEntry elements[];
};

struct DP_TransientCamera {
    DP_Atomic refcount;
    bool transient;
    bool hidden;
    uint8_t flags;
    uint8_t interpolation;
    int id;
    int framerate;
    int framerate_fraction;
    int range_first;
    int range_last;
    int output_width;
    int output_height;
    DP_Rect viewport;
    DP_Text *title;
    int count;
    DP_CameraEntry elements[];
};

#else

struct DP_Camera {
    DP_Atomic refcount;
    bool transient;
    bool hidden;
    uint8_t flags;
    uint8_t interpolation;
    int id;
    int framerate;
    int framerate_fraction;
    int range_first;
    int range_last;
    int output_width;
    int output_height;
    DP_Rect viewport;
    DP_Text *title;
    int count;
    DP_CameraEntry elements[];
};

#endif


static size_t camera_size(int count)
{
    return DP_FLEX_SIZEOF(DP_Camera, elements, DP_int_to_size(count));
}

static DP_TransientCamera *
camera_alloc(bool transient, bool hidden, int id, uint8_t flags,
             uint8_t interpolation, int framerate, int framerate_fraction,
             int range_first, int range_last, int output_width,
             int output_height, DP_Rect viewport, DP_Text *title_or_null,
             int count, const DP_CameraEntry *elements, int entry_count)
{
    DP_ASSERT(entry_count <= count);
    DP_ASSERT(elements || entry_count == 0);
    DP_TransientCamera *tc = DP_malloc(camera_size(count));
    DP_atomic_set(&tc->refcount, 1);
    tc->transient = transient;
    tc->hidden = hidden;
    tc->id = id;
    tc->flags = flags;
    tc->interpolation = interpolation;
    tc->framerate = framerate;
    tc->framerate_fraction = framerate_fraction;
    tc->range_first = range_first;
    tc->range_last = range_last;
    tc->output_width = output_width;
    tc->output_height = output_height;
    tc->viewport = viewport;
    tc->title = DP_text_incref_nullable(title_or_null);
    tc->count = count;
    for (int i = 0; i < entry_count; ++i) {
        const DP_CameraEntry *ce = &elements[i];
        tc->elements[i] = (DP_CameraEntry){
            ce->frame_index, {DP_camera_key_frame_incref(ce->ckf)}};
    }
    for (int i = entry_count; i < count; ++i) {
        tc->elements[i] = (DP_CameraEntry){-1, {NULL}};
    }
    return tc;
}


DP_Camera *DP_camera_incref(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    DP_atomic_inc(&c->refcount);
    return c;
}

DP_Camera *DP_camera_incref_nullable(DP_Camera *c_or_null)
{
    return c_or_null ? DP_camera_incref(c_or_null) : NULL;
}

void DP_camera_decref(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    if (DP_atomic_dec(&c->refcount)) {
        int count = c->count;
        for (int i = 0; i < count; ++i) {
            DP_camera_key_frame_decref_nullable(c->elements[i].ckf);
        }
        DP_text_decref_nullable(c->title);
        DP_free(c);
    }
}

void DP_camera_decref_nullable(DP_Camera *c_or_null)
{
    if (c_or_null) {
        DP_camera_decref(c_or_null);
    }
}

int DP_camera_refcount(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return DP_atomic_get(&c->refcount);
}

bool DP_camera_transient(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->transient;
}

bool DP_camera_hidden(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->hidden;
}

int DP_camera_id(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->id;
}

unsigned int DP_camera_flags(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->flags;
}

int DP_camera_interpolation(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->interpolation;
}

int DP_camera_framerate(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->framerate;
}

int DP_camera_framerate_fraction(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->framerate_fraction;
}

double DP_camera_effective_framerate(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return DP_document_metadata_effective_framerate_combine(
        c->framerate, c->framerate_fraction);
}

bool DP_camera_framerates_valid(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->framerate > 0 || c->framerate_fraction > 0;
}

int DP_camera_range_first(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->range_first;
}

int DP_camera_range_last(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->range_last;
}

bool DP_camera_range_valid(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    int range_first = c->range_first;
    int range_last = c->range_last;
    return range_first >= 0 && range_last >= 0 && range_first <= range_last;
}

int DP_camera_output_width(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->output_width;
}

int DP_camera_output_height(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->output_height;
}

bool DP_camera_output_valid(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->output_width > 0 && c->output_height > 0;
}

const DP_Rect *DP_camera_viewport(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return &c->viewport;
}

bool DP_camera_viewport_valid(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return !DP_rect_empty(c->viewport);
}

const char *DP_camera_title(DP_Camera *c, size_t *out_length)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return DP_text_string(c->title, out_length);
}

int DP_camera_key_frame_count(DP_Camera *c)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    return c->count;
}

int DP_camera_frame_index_at_noinc(DP_Camera *c, int index)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < c->count);
    return c->elements[index].frame_index;
}

DP_CameraKeyFrame *DP_camera_key_frame_at_noinc(DP_Camera *c, int index)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < c->count);
    return c->elements[index].ckf;
}


DP_TransientCamera *DP_transient_camera_new(DP_Camera *c, int reserve)
{
    DP_ASSERT(c);
    DP_ASSERT(DP_atomic_get(&c->refcount) > 0);
    DP_ASSERT(!c->transient);
    DP_ASSERT(reserve >= 0);
    int count = c->count;
    DP_TransientCamera *tc = camera_alloc(
        true, c->hidden, c->id, c->flags, c->interpolation, c->framerate,
        c->framerate_fraction, c->range_first, c->range_last, c->output_width,
        c->output_height, c->viewport, c->title, count + reserve, c->elements,
        count);
    return tc;
}

DP_TransientCamera *DP_transient_camera_new_init(int id, int reserve)
{
    DP_ASSERT(reserve >= 0);
    DP_TransientCamera *tc =
        camera_alloc(true, false, id, 0, 0, 0, 0, -1, -1, 0, 0,
                     INVALID_VIEWPORT, NULL, reserve, NULL, 0);
    return tc;
}

DP_TransientCamera *DP_transient_camera_reserve(DP_TransientCamera *tc,
                                                int reserve)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d key frames in camera", reserve);
    if (reserve > 0) {
        int old_count = tc->count;
        int new_count = old_count + reserve;
        tc = DP_realloc(tc, camera_size(new_count));
        tc->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tc->elements[i] = (DP_CameraEntry){-1, {NULL}};
        }
    }
    return tc;
}

DP_TransientCamera *DP_transient_camera_incref(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return (DP_TransientCamera *)DP_camera_incref((DP_Camera *)tc);
}

void DP_transient_camera_decref(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    DP_camera_decref((DP_Camera *)tc);
}

int DP_transient_camera_refcount(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_refcount((DP_Camera *)tc);
}

DP_Camera *DP_transient_camera_persist(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->transient = false;
    return (DP_Camera *)tc;
}

int DP_transient_camera_id(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_id((DP_Camera *)tc);
}

unsigned int DP_transient_camera_flags(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_flags((DP_Camera *)tc);
}

int DP_transient_camera_interpolation(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_interpolation((DP_Camera *)tc);
}

int DP_transient_camera_framerate(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_framerate((DP_Camera *)tc);
}

int DP_transient_camera_framerate_fraction(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_framerate_fraction((DP_Camera *)tc);
}

double DP_transient_camera_effective_framerate(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_effective_framerate((DP_Camera *)tc);
}

bool DP_transient_camera_framerates_valid(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_framerates_valid((DP_Camera *)tc);
}

int DP_transient_camera_range_first(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_range_first((DP_Camera *)tc);
}

int DP_transient_camera_range_last(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_range_last((DP_Camera *)tc);
}

bool DP_transient_camera_range_valid(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_range_valid((DP_Camera *)tc);
}

int DP_transient_camera_output_width(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_output_width((DP_Camera *)tc);
}

int DP_transient_camera_output_height(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_output_height((DP_Camera *)tc);
}

bool DP_transient_camera_output_valid(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_output_valid((DP_Camera *)tc);
}

const DP_Rect *DP_transient_camera_viewport(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_viewport((DP_Camera *)tc);
}

bool DP_transient_camera_viewport_valid(DP_TransientCamera *tc)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_viewport_valid((DP_Camera *)tc);
}

const char *DP_transient_camera_title(DP_TransientCamera *tc,
                                      size_t *out_length)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    return DP_camera_title((DP_Camera *)tc, out_length);
}

void DP_transient_camera_id_set(DP_TransientCamera *tc, int id)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->id = id;
}

void DP_transient_camera_flags_set(DP_TransientCamera *tc, unsigned int flags)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->flags = DP_uint_to_uint8(flags);
}

void DP_transient_camera_interpolation_set(DP_TransientCamera *tc,
                                           int interpolation)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->interpolation = DP_int_to_uint8(interpolation);
}

void DP_transient_camera_framerate_set(DP_TransientCamera *tc, int framerate)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->framerate = framerate;
}

void DP_transient_camera_framerate_fraction_set(DP_TransientCamera *tc,
                                                int framerate_fraction)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->framerate_fraction = framerate_fraction;
}

void DP_transient_camera_range_first_set(DP_TransientCamera *tc,
                                         int range_first)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->range_first = range_first;
}

void DP_transient_camera_range_last_set(DP_TransientCamera *tc, int range_last)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->range_last = range_last;
}

void DP_transient_camera_output_width_set(DP_TransientCamera *tc,
                                          int output_width)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->output_width = output_width;
}

void DP_transient_camera_output_height_set(DP_TransientCamera *tc,
                                           int output_height)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->output_height = output_height;
}

void DP_transient_camera_viewport_set(DP_TransientCamera *tc,
                                      const DP_Rect *viewport)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    tc->viewport =
        viewport && !DP_rect_empty(*viewport) ? *viewport : INVALID_VIEWPORT;
}

void DP_transient_camera_title_set(DP_TransientCamera *tc, const char *title,
                                   size_t length)
{
    DP_ASSERT(tc);
    DP_ASSERT(DP_atomic_get(&tc->refcount) > 0);
    DP_ASSERT(tc->transient);
    DP_text_decref_nullable(tc->title);
    tc->title = DP_text_new(title, length);
}
