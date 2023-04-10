// SPDX-License-Identifier: MIT
#ifndef DPENGINE_KEY_FRAME_H
#define DPENGINE_KEY_FRAME_H
#include <dpcommon/common.h>

#define DP_KEY_FRAME_LAYER_HIDDEN   (1 << 0)
#define DP_KEY_FRAME_LAYER_REVEALED (1 << 1)

typedef struct DP_KeyFrame DP_KeyFrame;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientKeyFrame DP_TransientKeyFrame;
#else
typedef struct DP_KeyFrame DP_TransientKeyFrame;
#endif

typedef struct DP_KeyFrameLayer {
    int layer_id;
    unsigned int flags;
} DP_KeyFrameLayer;

bool DP_key_frame_layer_hidden(const DP_KeyFrameLayer *kfl);

bool DP_key_frame_layer_revealed(const DP_KeyFrameLayer *kfl);


DP_KeyFrame *DP_key_frame_new_init(int layer_id);

DP_KeyFrame *DP_key_frame_incref(DP_KeyFrame *kf);

DP_KeyFrame *DP_key_frame_incref_nullable(DP_KeyFrame *kf_or_null);

void DP_key_frame_decref(DP_KeyFrame *kf);

void DP_key_frame_decref_nullable(DP_KeyFrame *kf_or_null);

int DP_key_frame_refcount(DP_KeyFrame *kf);

bool DP_key_frame_transient(DP_KeyFrame *kf);

const char *DP_key_frame_title(DP_KeyFrame *kf, size_t *out_length);

int DP_key_frame_layer_id(DP_KeyFrame *kf);

const DP_KeyFrameLayer *DP_key_frame_layers(DP_KeyFrame *kf, int *out_count);

bool DP_key_frame_same_frame(DP_KeyFrame *kf_a, DP_KeyFrame *kf_b);


DP_TransientKeyFrame *DP_transient_key_frame_new(DP_KeyFrame *kf);

DP_TransientKeyFrame *DP_transient_key_frame_new_with_layers(
    DP_KeyFrame *kf, const DP_KeyFrameLayer *layers, int count);

DP_TransientKeyFrame *DP_transient_key_frame_new_init(int layer_id,
                                                      int reserve);

DP_TransientKeyFrame *DP_transient_key_frame_incref(DP_TransientKeyFrame *tkf);

void DP_transient_key_frame_decref(DP_TransientKeyFrame *tkf);

int DP_transient_key_frame_refcount(DP_TransientKeyFrame *tkf);

DP_KeyFrame *DP_transient_key_frame_persist(DP_TransientKeyFrame *tkf);

void DP_transient_key_frame_title_set(DP_TransientKeyFrame *tkf,
                                      const char *title, size_t length);

void DP_transient_key_frame_layer_id_set(DP_TransientKeyFrame *tkf,
                                         int layer_id);

void DP_transient_key_frame_layer_set(DP_TransientKeyFrame *tkf,
                                      DP_KeyFrameLayer kfl, int index);

void DP_transient_key_frame_layer_delete_at(DP_TransientKeyFrame *tkf,
                                            int index);


#endif
