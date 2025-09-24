// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_CAMERA_H
#define DPENGINE_CAMERA_H
#include <dpcommon/common.h>

typedef struct DP_Rect DP_Rect;
DP_TYPEDEF_PERSISTENT(Camera);
DP_TYPEDEF_PERSISTENT(CameraKeyFrame);

// Whether to oscillate these shake parameters instead of using random noise.
#define DP_CAMERA_FLAG_SHAKE_OFFSET_X_OSCILLATE (1u << 0u)
#define DP_CAMERA_FLAG_SHAKE_OFFSET_Y_OSCILLATE (1u << 1u)
#define DP_CAMERA_FLAG_SHAKE_ROTATION_OSCILLATE (1u << 1u)

DP_Camera *DP_camera_incref(DP_Camera *c);

DP_Camera *DP_camera_incref_nullable(DP_Camera *c_or_null);

void DP_camera_decref(DP_Camera *c);

void DP_camera_decref_nullable(DP_Camera *c_or_null);

int DP_camera_refcount(DP_Camera *c);

bool DP_camera_transient(DP_Camera *c);

bool DP_camera_hidden(DP_Camera *c);

int DP_camera_id(DP_Camera *c);

unsigned int DP_camera_flags(DP_Camera *c);

int DP_camera_interpolation(DP_Camera *c);

int DP_camera_framerate(DP_Camera *c);
int DP_camera_framerate_fraction(DP_Camera *c);
double DP_camera_effective_framerate(DP_Camera *c);
bool DP_camera_framerates_valid(DP_Camera *c);

int DP_camera_range_first(DP_Camera *c);
int DP_camera_range_last(DP_Camera *c);
bool DP_camera_range_valid(DP_Camera *c);

int DP_camera_output_width(DP_Camera *c);
int DP_camera_output_height(DP_Camera *c);
bool DP_camera_output_valid(DP_Camera *c);

const DP_Rect *DP_camera_viewport(DP_Camera *c);
bool DP_camera_viewport_valid(DP_Camera *c);

const char *DP_camera_title(DP_Camera *c, size_t *out_length);

int DP_camera_key_frame_count(DP_Camera *c);

int DP_camera_frame_index_at_noinc(DP_Camera *c, int index);

DP_CameraKeyFrame *DP_camera_key_frame_at_noinc(DP_Camera *c, int index);

DP_CameraKeyFrame *DP_camera_key_frame_at_inc(DP_Camera *c, int index);


DP_TransientCamera *DP_transient_camera_new(DP_Camera *c, int reserve);

DP_TransientCamera *DP_transient_camera_new_init(int id, int reserve);

DP_TransientCamera *DP_transient_camera_reserve(DP_TransientCamera *tc,
                                                int reserve);

DP_TransientCamera *DP_transient_camera_incref(DP_TransientCamera *tc);

void DP_transient_camera_decref(DP_TransientCamera *tc);

int DP_transient_camera_refcount(DP_TransientCamera *tc);

DP_Camera *DP_transient_camera_persist(DP_TransientCamera *tc);

int DP_transient_camera_id(DP_TransientCamera *tc);

unsigned int DP_transient_camera_flags(DP_TransientCamera *tc);

int DP_transient_camera_interpolation(DP_TransientCamera *tc);

int DP_transient_camera_framerate(DP_TransientCamera *tc);
int DP_transient_camera_framerate_fraction(DP_TransientCamera *tc);
double DP_transient_camera_effective_framerate(DP_TransientCamera *tc);
bool DP_transient_camera_framerates_valid(DP_TransientCamera *tc);

int DP_transient_camera_range_first(DP_TransientCamera *tc);
int DP_transient_camera_range_last(DP_TransientCamera *tc);
bool DP_transient_camera_range_valid(DP_TransientCamera *tc);

int DP_transient_camera_output_width(DP_TransientCamera *tc);
int DP_transient_camera_output_height(DP_TransientCamera *tc);
bool DP_transient_camera_output_valid(DP_TransientCamera *tc);

const DP_Rect *DP_transient_camera_viewport(DP_TransientCamera *tc);
bool DP_transient_camera_viewport_valid(DP_TransientCamera *tc);

const char *DP_transient_camera_title(DP_TransientCamera *tc,
                                      size_t *out_length);

void DP_transient_camera_id_set(DP_TransientCamera *tc, int id);

void DP_transient_camera_flags_set(DP_TransientCamera *tc, unsigned int flags);

void DP_transient_camera_interpolation_set(DP_TransientCamera *tc,
                                           int interpolation);

void DP_transient_camera_framerate_set(DP_TransientCamera *tc, int framerate);

void DP_transient_camera_framerate_fraction_set(DP_TransientCamera *tc,
                                                int framerate_fraction);

void DP_transient_camera_range_first_set(DP_TransientCamera *tc,
                                         int range_first);

void DP_transient_camera_range_last_set(DP_TransientCamera *tc, int range_last);

void DP_transient_camera_output_width_set(DP_TransientCamera *tc,
                                          int output_width);

void DP_transient_camera_output_height_set(DP_TransientCamera *tc,
                                           int output_height);

void DP_transient_camera_viewport_set(DP_TransientCamera *tc,
                                      const DP_Rect *viewport);

void DP_transient_camera_title_set(DP_TransientCamera *tc, const char *title,
                                   size_t length);


#endif
