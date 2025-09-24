// SPDX-License-Identifier: GPL-3.0-or-later
#include "camera_key_frame.h"
#include "text.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/curve.h>


typedef struct DP_CameraKeyFrameEntry {
    int type;
    int prop;
    union {
        double value;
        DP_Curve *curve;
    };
} DP_CameraKeyFrameEntry;

#ifdef DP_NO_STRICT_ALIASING

struct DP_CameraKeyFrame {
    DP_Atomic refcount;
    const bool transient;
    DP_Text *const title;
    const int count;
    const DP_CameraKeyFrameEntry elements[];
};

struct DP_TransientCameraKeyFrame {
    DP_Atomic refcount;
    bool transient;
    DP_Text *title;
    int count;
    DP_CameraKeyFrameEntry elements[];
};

#else

struct DP_CameraKeyFrame {
    DP_Atomic refcount;
    bool transient;
    DP_Text *title;
    int count;
    DP_CameraKeyFrameEntry elements[];
};

#endif


static DP_CameraKeyFrameEntry null_entry(void)
{
    return (DP_CameraKeyFrameEntry){DP_CAMERA_KEY_FRAME_PROP_TYPE_INVALID,
                                    DP_CAMERA_KEY_FRAME_PROP_INVALID,
                                    {.value = 0.0}};
}

static DP_CameraKeyFrameEntry entry_incref(DP_CameraKeyFrameEntry entry)
{
    if (entry.type == DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE) {
        DP_curve_incref(entry.curve);
    }
    return entry;
}

static DP_CameraKeyFrameEntry
entry_incref_nullable(DP_CameraKeyFrameEntry entry)
{
    if (entry.type == DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE) {
        DP_curve_incref_nullable(entry.curve);
    }
    return entry;
}

static void entry_decref(DP_CameraKeyFrameEntry entry)
{
    if (entry.type == DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE) {
        DP_curve_decref(entry.curve);
    }
}

static void entry_decref_nullable(DP_CameraKeyFrameEntry entry)
{
    if (entry.type == DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE) {
        DP_curve_decref_nullable(entry.curve);
    }
}

static size_t camera_key_frame_size(int count)
{
    return DP_FLEX_SIZEOF(DP_CameraKeyFrame, elements, DP_int_to_size(count));
}

static void *allocate_camera_key_frame(bool transient, DP_Text *title_or_null,
                                       int count,
                                       const DP_CameraKeyFrameEntry *elements,
                                       int entry_count)
{
    DP_ASSERT(entry_count <= count);
    DP_ASSERT(elements || entry_count == 0);
    DP_TransientCameraKeyFrame *tckf = DP_malloc(camera_key_frame_size(count));
    DP_atomic_set(&tckf->refcount, 1);
    tckf->transient = transient;
    tckf->title = DP_text_incref_nullable(title_or_null);
    tckf->count = count;
    for (int i = 0; i < entry_count; ++i) {
        tckf->elements[i] = entry_incref(elements[i]);
    }
    for (int i = entry_count; i < count; ++i) {
        tckf->elements[i] = null_entry();
    }
    return tckf;
}


DP_CameraKeyFrame *DP_camera_key_frame_new(void)
{
    return allocate_camera_key_frame(false, NULL, 0, NULL, 0);
}

DP_CameraKeyFrame *DP_camera_key_frame_incref(DP_CameraKeyFrame *ckf)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    DP_atomic_inc(&ckf->refcount);
    return ckf;
}

DP_CameraKeyFrame *
DP_camera_key_frame_incref_nullable(DP_CameraKeyFrame *ckf_or_null)
{
    return ckf_or_null ? DP_camera_key_frame_incref(ckf_or_null) : NULL;
}

void DP_camera_key_frame_decref(DP_CameraKeyFrame *ckf)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    if (DP_atomic_dec(&ckf->refcount)) {
        int count = ckf->count;
        for (int i = 0; i < count; ++i) {
            entry_decref_nullable(ckf->elements[i]);
        }
        DP_text_decref_nullable(ckf->title);
        DP_free(ckf);
    }
}

void DP_camera_key_frame_decref_nullable(DP_CameraKeyFrame *ckf_or_null)
{
    if (ckf_or_null) {
        DP_camera_key_frame_decref(ckf_or_null);
    }
}

int DP_camera_key_frame_refcount(DP_CameraKeyFrame *ckf)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    return DP_atomic_get(&ckf->refcount);
}

bool DP_camera_key_frame_transient(DP_CameraKeyFrame *ckf)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    return ckf->transient;
}

const char *DP_camera_key_frame_title(DP_CameraKeyFrame *ckf,
                                      size_t *out_length)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    return DP_text_string(ckf->title, out_length);
}

int DP_camera_key_frame_prop_count(DP_CameraKeyFrame *ckf)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    return ckf->count;
}

static int camera_element_index(DP_CameraKeyFrame *ckf, int type, int prop)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    int count = ckf->count;
    for (int i = 0; i < count; ++i) {
        const DP_CameraKeyFrameEntry *entry = &ckf->elements[i];
        if (entry->type == type && entry->prop == prop) {
            return i;
        }
    }
    return -1;
}

int DP_camera_key_frame_value_index(DP_CameraKeyFrame *ckf, int prop)
{
    return camera_element_index(ckf, DP_CAMERA_KEY_FRAME_PROP_TYPE_VALUE, prop);
}

int DP_camera_key_frame_curve_index(DP_CameraKeyFrame *ckf, int prop)
{
    return camera_element_index(ckf, DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE, prop);
}

static const DP_CameraKeyFrameEntry *camera_entry_at(DP_CameraKeyFrame *ckf,
                                                     int index)
{
    DP_ASSERT(ckf);
    DP_ASSERT(DP_atomic_get(&ckf->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ckf->count);
    return &ckf->elements[index];
}

int DP_camera_key_frame_prop_type_at(DP_CameraKeyFrame *ckf, int index)
{
    const DP_CameraKeyFrameEntry *entry = camera_entry_at(ckf, index);
    return entry->type;
}

double DP_camera_key_frame_value_at(DP_CameraKeyFrame *ckf, int index)
{
    const DP_CameraKeyFrameEntry *entry = camera_entry_at(ckf, index);
    DP_ASSERT(entry->type == DP_CAMERA_KEY_FRAME_PROP_TYPE_VALUE);
    return entry->value;
}

DP_Curve *DP_camera_key_frame_curve_at_noinc(DP_CameraKeyFrame *ckf, int index)
{
    const DP_CameraKeyFrameEntry *entry = camera_entry_at(ckf, index);
    DP_ASSERT(entry->type == DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE);
    return entry->curve;
}


DP_TransientCameraKeyFrame *
DP_transient_camera_key_frame_new(DP_CameraKeyFrame *ckf, int reserve)
{
    DP_ASSERT(ckf);
    DP_ASSERT(reserve >= 0);
    int entry_count = ckf->count;
    return allocate_camera_key_frame(true, ckf->title, entry_count + reserve,
                                     ckf->elements, entry_count);
}

DP_TransientCameraKeyFrame *DP_transient_camera_key_frame_new_init(int reserve)
{
    DP_ASSERT(reserve >= 0);
    return allocate_camera_key_frame(true, NULL, reserve, NULL, 0);
}

DP_TransientCameraKeyFrame *
DP_transient_camera_key_frame_reserve(DP_TransientCameraKeyFrame *tckf,
                                      int reserve)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d props in camera key frame", reserve);
    if (reserve > 0) {
        int old_count = tckf->count;
        int new_count = old_count + reserve;
        tckf = DP_realloc(tckf, camera_key_frame_size(new_count));
        tckf->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tckf->elements[i] = null_entry();
        }
    }
    return tckf;
}

DP_TransientCameraKeyFrame *
DP_transient_camera_key_frame_incref(DP_TransientCameraKeyFrame *tckf)
{
    return (DP_TransientCameraKeyFrame *)DP_camera_key_frame_incref(
        (DP_CameraKeyFrame *)tckf);
}

void DP_transient_camera_key_frame_decref(DP_TransientCameraKeyFrame *tckf)
{
    DP_camera_key_frame_decref((DP_CameraKeyFrame *)tckf);
}

int DP_transient_camera_key_frame_refcount(DP_TransientCameraKeyFrame *tckf)
{
    return DP_camera_key_frame_refcount((DP_CameraKeyFrame *)tckf);
}

DP_CameraKeyFrame *
DP_transient_camera_key_frame_persist(DP_TransientCameraKeyFrame *tckf)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    tckf->transient = false;
    return (DP_CameraKeyFrame *)tckf;
}

const char *
DP_transient_camera_key_frame_title(DP_TransientCameraKeyFrame *tckf,
                                    size_t *out_length)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_title((DP_CameraKeyFrame *)tckf, out_length);
}

int DP_transient_camera_key_frame_prop_count(DP_TransientCameraKeyFrame *tckf)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_prop_count((DP_CameraKeyFrame *)tckf);
}

int DP_transient_camera_key_frame_value_index(DP_TransientCameraKeyFrame *tckf,
                                              int prop)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_value_index((DP_CameraKeyFrame *)tckf, prop);
}

int DP_transient_camera_key_frame_curve_index(DP_TransientCameraKeyFrame *tckf,
                                              int prop)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_curve_index((DP_CameraKeyFrame *)tckf, prop);
}

int DP_transient_camera_key_frame_prop_type_at(DP_TransientCameraKeyFrame *tckf,
                                               int index)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_prop_type_at((DP_CameraKeyFrame *)tckf, index);
}

double DP_transient_camera_key_frame_value_at(DP_TransientCameraKeyFrame *tckf,
                                              int index)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_value_at((DP_CameraKeyFrame *)tckf, index);
}

DP_Curve *
DP_transient_camera_key_frame_curve_at_noinc(DP_TransientCameraKeyFrame *tckf,
                                             int index)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    return DP_camera_key_frame_curve_at_noinc((DP_CameraKeyFrame *)tckf, index);
}

void DP_transient_camera_key_frame_title_set(DP_TransientCameraKeyFrame *tckf,
                                             const char *title, size_t length)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    DP_text_decref_nullable(tckf->title);
    tckf->title = DP_text_new(title, length);
}

// Assertion to make sure there isn't two entries with the same type and prop.
#ifdef NDEBUG
#    define TRANSIENT_CAMERA_ASSERT_PROP_SET(TCKF, INDEX, TYPE, \
                                             PROP) /*nothing*/
#else
#    define TRANSIENT_CAMERA_ASSERT_PROP_SET(TCKF, INDEX, TYPE, PROP)        \
        do {                                                                 \
            DP_TransientCameraKeyFrame *_tckf = (TCKF);                      \
            int _index = (INDEX);                                            \
            int _type = (TYPE);                                              \
            int _prop = (PROP);                                              \
            int _count = _tckf->count;                                       \
            for (int _i = 0; _i < _count; ++_i) {                            \
                if (_i != _index) {                                          \
                    const DP_CameraKeyFrameEntry *_e = &_tckf->elements[_i]; \
                    DP_ASSERT(_e->type != _type || _e->prop != _prop);       \
                }                                                            \
            }                                                                \
        } while (0)
#endif

void DP_transient_camera_key_frame_value_set_at(
    DP_TransientCameraKeyFrame *tckf, int index, int prop, double value)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tckf->count);
    TRANSIENT_CAMERA_ASSERT_PROP_SET(tckf, index,
                                     DP_CAMERA_KEY_FRAME_PROP_TYPE_VALUE, prop);
    entry_decref_nullable(tckf->elements[index]);
    tckf->elements[index] = (DP_CameraKeyFrameEntry){
        DP_CAMERA_KEY_FRAME_PROP_TYPE_VALUE, prop, {.value = value}};
}

void DP_transient_camera_key_frame_curve_set_at_noinc(
    DP_TransientCameraKeyFrame *tckf, int index, int prop, DP_Curve *curve)
{
    DP_ASSERT(tckf);
    DP_ASSERT(DP_atomic_get(&tckf->refcount) > 0);
    DP_ASSERT(tckf->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tckf->count);
    DP_ASSERT(curve);
    TRANSIENT_CAMERA_ASSERT_PROP_SET(tckf, index,
                                     DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE, prop);
    entry_decref_nullable(tckf->elements[index]);
    tckf->elements[index] = (DP_CameraKeyFrameEntry){
        DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE, prop, {.curve = curve}};
}

void DP_transient_camera_key_frame_curve_set_at_inc(
    DP_TransientCameraKeyFrame *tckf, int index, int prop, DP_Curve *curve)
{
    DP_transient_camera_key_frame_curve_set_at_noinc(tckf, index, prop,
                                                     DP_curve_incref(curve));
}

void DP_transient_camera_key_frame_delete_at(DP_TransientCameraKeyFrame *tlpl,
                                             int index)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    entry_decref(tlpl->elements[index]);
    int new_count = --tlpl->count;
    memmove(&tlpl->elements[index], &tlpl->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tlpl->elements[0]));
}
