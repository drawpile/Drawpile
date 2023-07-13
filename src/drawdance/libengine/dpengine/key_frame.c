// SPDX-License-Identifier: MIT
#include "key_frame.h"
#include "text.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define LAYER_VISIBILITY_MASK \
    (DP_KEY_FRAME_LAYER_HIDDEN | DP_KEY_FRAME_LAYER_REVEALED)

#ifdef DP_NO_STRICT_ALIASING

struct DP_KeyFrame {
    DP_Atomic refcount;
    const bool transient;
    DP_Text *const title;
    const int layer_id;
    const int count;
    const DP_KeyFrameLayer layers[];
};

struct DP_TransientKeyFrame {
    DP_Atomic refcount;
    bool transient;
    DP_Text *title;
    int layer_id;
    int count;
    DP_KeyFrameLayer layers[];
};

#else

struct DP_KeyFrame {
    DP_Atomic refcount;
    bool transient;
    DP_Text *title;
    int layer_id;
    int count;
    DP_KeyFrameLayer layers[];
};

#endif


bool DP_key_frame_layer_hidden(const DP_KeyFrameLayer *kfl)
{
    DP_ASSERT(kfl);
    return (kfl->flags & LAYER_VISIBILITY_MASK) == DP_KEY_FRAME_LAYER_HIDDEN;
}

bool DP_key_frame_layer_revealed(const DP_KeyFrameLayer *kfl)
{
    DP_ASSERT(kfl);
    return (kfl->flags & LAYER_VISIBILITY_MASK) == DP_KEY_FRAME_LAYER_REVEALED;
}


static void *allocate_key_frame(bool transient, int count)
{
    DP_TransientKeyFrame *tkf = DP_malloc(
        DP_FLEX_SIZEOF(DP_TransientKeyFrame, layers, DP_int_to_size(count)));
    DP_atomic_set(&tkf->refcount, 1);
    tkf->transient = transient;
    tkf->count = count;
    return tkf;
}


DP_KeyFrame *DP_key_frame_new_init(int layer_id)
{
    DP_TransientKeyFrame *tkf = allocate_key_frame(false, 0);
    tkf->layer_id = layer_id;
    tkf->title = NULL;
    return (DP_KeyFrame *)tkf;
}

DP_KeyFrame *DP_key_frame_incref(DP_KeyFrame *kf)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    DP_atomic_inc(&kf->refcount);
    return kf;
}

DP_KeyFrame *DP_key_frame_incref_nullable(DP_KeyFrame *kf_or_null)
{
    return kf_or_null ? DP_key_frame_incref(kf_or_null) : NULL;
}

void DP_key_frame_decref(DP_KeyFrame *kf)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    if (DP_atomic_dec(&kf->refcount)) {
        DP_text_decref_nullable(kf->title);
        DP_free(kf);
    }
}

void DP_key_frame_decref_nullable(DP_KeyFrame *kf_or_null)
{
    if (kf_or_null) {
        DP_key_frame_decref(kf_or_null);
    }
}

int DP_key_frame_refcount(DP_KeyFrame *kf)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    return DP_atomic_get(&kf->refcount);
}

bool DP_key_frame_transient(DP_KeyFrame *kf)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    return kf->transient;
}

const char *DP_key_frame_title(DP_KeyFrame *kf, size_t *out_length)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    return DP_text_string(kf->title, out_length);
}

int DP_key_frame_layer_id(DP_KeyFrame *kf)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    return kf->layer_id;
}

const DP_KeyFrameLayer *DP_key_frame_layers(DP_KeyFrame *kf, int *out_count)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    if (out_count) {
        *out_count = kf->count;
    }
    return kf->layers;
}

static bool has_same_key_frame_layer(DP_KeyFrame *kf, DP_KeyFrameLayer kfl_a)
{
    int layer_count = kf->count;
    for (int i = 0; i < layer_count; ++i) {
        DP_KeyFrameLayer kfl_b = kf->layers[i];
        if (kfl_a.layer_id == kfl_b.layer_id) {
            return kfl_a.flags == kfl_b.flags;
        }
    }
    return false;
}

bool DP_key_frame_same_frame(DP_KeyFrame *kf_a, DP_KeyFrame *kf_b)
{
    DP_ASSERT(!kf_a || DP_atomic_get(&kf_a->refcount) > 0);
    DP_ASSERT(!kf_b || DP_atomic_get(&kf_b->refcount) > 0);
    bool a_blank = !kf_a || kf_a->layer_id == 0;
    bool b_blank = !kf_b || kf_b->layer_id == 0;
    if (a_blank && b_blank) {
        return true;
    }
    else if (a_blank ^ b_blank || kf_a->layer_id != kf_b->layer_id) {
        return false;
    }
    else {
        int layer_count = kf_a->count;
        for (int i = 0; i < layer_count; ++i) {
            if (!has_same_key_frame_layer(kf_b, kf_a->layers[i])) {
                return false;
            }
        }
        return true;
    }
}


DP_TransientKeyFrame *DP_transient_key_frame_new(DP_KeyFrame *kf)
{
    DP_ASSERT(kf);
    DP_ASSERT(DP_atomic_get(&kf->refcount) > 0);
    DP_ASSERT(!kf->transient);
    return DP_transient_key_frame_new_with_layers(kf, kf->layers, kf->count);
}

DP_TransientKeyFrame *DP_transient_key_frame_new_with_layers(
    DP_KeyFrame *kf, const DP_KeyFrameLayer *layers, int count)
{
    DP_TransientKeyFrame *tkf = allocate_key_frame(true, count);
    tkf->title = DP_text_incref_nullable(kf->title);
    tkf->layer_id = kf->layer_id;
    tkf->count = count;
    memcpy(tkf->layers, layers, sizeof(*layers) * DP_int_to_size(count));
    return tkf;
}

DP_TransientKeyFrame *DP_transient_key_frame_new_init(int layer_id, int reserve)
{
    DP_TransientKeyFrame *tkf = allocate_key_frame(true, reserve);
    tkf->title = NULL;
    tkf->layer_id = layer_id;
    for (int i = 0; i < reserve; ++i) {
        tkf->layers[i] = (DP_KeyFrameLayer){0, 0};
    }
    return tkf;
}

DP_TransientKeyFrame *DP_transient_key_frame_incref(DP_TransientKeyFrame *tt)
{
    return (DP_TransientKeyFrame *)DP_key_frame_incref((DP_KeyFrame *)tt);
}

void DP_transient_key_frame_decref(DP_TransientKeyFrame *tkf)
{
    DP_key_frame_decref((DP_KeyFrame *)tkf);
}

int DP_transient_key_frame_refcount(DP_TransientKeyFrame *tt)
{
    return DP_key_frame_refcount((DP_KeyFrame *)tt);
}

DP_KeyFrame *DP_transient_key_frame_persist(DP_TransientKeyFrame *tkf)
{
    DP_ASSERT(tkf);
    DP_ASSERT(DP_atomic_get(&tkf->refcount) > 0);
    DP_ASSERT(tkf->transient);
    tkf->transient = false;
    return (DP_KeyFrame *)tkf;
}

void DP_transient_key_frame_title_set(DP_TransientKeyFrame *tkf,
                                      const char *title, size_t length)
{
    DP_ASSERT(tkf);
    DP_ASSERT(DP_atomic_get(&tkf->refcount) > 0);
    DP_ASSERT(tkf->transient);
    DP_text_decref_nullable(tkf->title);
    tkf->title = DP_text_new(title, length);
}

void DP_transient_key_frame_layer_id_set(DP_TransientKeyFrame *tkf,
                                         int layer_id)
{
    DP_ASSERT(tkf);
    DP_ASSERT(DP_atomic_get(&tkf->refcount) > 0);
    DP_ASSERT(tkf->transient);
    tkf->layer_id = layer_id;
}

void DP_transient_key_frame_layer_set(DP_TransientKeyFrame *tkf,
                                      DP_KeyFrameLayer kfl, int index)
{
    DP_ASSERT(tkf);
    DP_ASSERT(DP_atomic_get(&tkf->refcount) > 0);
    DP_ASSERT(tkf->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tkf->count);
    tkf->layers[index] = kfl;
}

void DP_transient_key_frame_layer_delete_at(DP_TransientKeyFrame *tkf,
                                            int index)
{
    DP_ASSERT(tkf);
    DP_ASSERT(DP_atomic_get(&tkf->refcount) > 0);
    DP_ASSERT(tkf->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tkf->count);
    int new_count = --tkf->count;
    memmove(&tkf->layers[index], &tkf->layers[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tkf->layers[0]));
}
