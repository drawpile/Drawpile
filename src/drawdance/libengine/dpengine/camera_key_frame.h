// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_CAMERA_KEY_FRAME_H
#define DPENGINE_CAMERA_KEY_FRAME_H
#include <dpcommon/common.h>

typedef struct DP_Curve DP_Curve;

DP_TYPEDEF_PERSISTENT(CameraKeyFrame);

typedef enum DP_CameraKeyFramePropType {
    DP_CAMERA_KEY_FRAME_PROP_TYPE_INVALID,
    DP_CAMERA_KEY_FRAME_PROP_TYPE_VALUE,
    DP_CAMERA_KEY_FRAME_PROP_TYPE_CURVE,
} DP_CameraKeyFramePropType;

typedef enum DP_CameraKeyFrameProp {
    DP_CAMERA_KEY_FRAME_PROP_INVALID,
    DP_CAMERA_KEY_FRAME_PROP_POS_X,
    DP_CAMERA_KEY_FRAME_PROP_POS_Y,
    DP_CAMERA_KEY_FRAME_PROP_SCALE_X,
    DP_CAMERA_KEY_FRAME_PROP_SCALE_Y,
    DP_CAMERA_KEY_FRAME_PROP_ROTATION,
    DP_CAMERA_KEY_FRAME_PROP_TINT_OPACITY,
    DP_CAMERA_KEY_FRAME_PROP_TINT_RGB,
    DP_CAMERA_KEY_FRAME_PROP_SHAKE_INTENSITY,
    DP_CAMERA_KEY_FRAME_PROP_SHAKE_SPEED,
    DP_CAMERA_KEY_FRAME_PROP_SHAKE_OFFSET_X,
    DP_CAMERA_KEY_FRAME_PROP_SHAKE_OFFSET_Y,
    DP_CAMERA_KEY_FRAME_PROP_SHAKE_ROTATION,
    DP_CAMERA_KEY_FRAME_PROP_COUNT,
} DP_CameraKeyFrameProp;


DP_CameraKeyFrame *DP_camera_key_frame_new(void);

DP_CameraKeyFrame *DP_camera_key_frame_incref(DP_CameraKeyFrame *ckf);

DP_CameraKeyFrame *
DP_camera_key_frame_incref_nullable(DP_CameraKeyFrame *ckf_or_null);

void DP_camera_key_frame_decref(DP_CameraKeyFrame *ckf);

void DP_camera_key_frame_decref_nullable(DP_CameraKeyFrame *ckf_or_null);

int DP_camera_key_frame_refcount(DP_CameraKeyFrame *ckf);

bool DP_camera_key_frame_transient(DP_CameraKeyFrame *ckf);

const char *DP_camera_key_frame_title(DP_CameraKeyFrame *ckf,
                                      size_t *out_length);

int DP_camera_key_frame_prop_count(DP_CameraKeyFrame *ckf);

int DP_camera_key_frame_value_index(DP_CameraKeyFrame *ckf, int prop);

int DP_camera_key_frame_curve_index(DP_CameraKeyFrame *ckf, int prop);

int DP_camera_key_frame_prop_type_at(DP_CameraKeyFrame *ckf, int index);

double DP_camera_key_frame_value_at(DP_CameraKeyFrame *ckf, int index);

DP_Curve *DP_camera_key_frame_curve_at_noinc(DP_CameraKeyFrame *ckf, int index);


DP_TransientCameraKeyFrame *
DP_transient_camera_key_frame_new(DP_CameraKeyFrame *ckf, int reserve);

DP_TransientCameraKeyFrame *DP_transient_camera_key_frame_new_init(int reserve);

DP_TransientCameraKeyFrame *
DP_transient_camera_key_frame_reserve(DP_TransientCameraKeyFrame *tckf,
                                      int reserve);

DP_TransientCameraKeyFrame *
DP_transient_camera_key_frame_incref(DP_TransientCameraKeyFrame *tckf);

void DP_transient_camera_key_frame_decref(DP_TransientCameraKeyFrame *tckf);

int DP_transient_camera_key_frame_refcount(DP_TransientCameraKeyFrame *tckf);

DP_CameraKeyFrame *
DP_transient_camera_key_frame_persist(DP_TransientCameraKeyFrame *tckf);

const char *
DP_transient_camera_key_frame_title(DP_TransientCameraKeyFrame *tckf,
                                    size_t *out_length);

int DP_transient_camera_key_frame_prop_count(DP_TransientCameraKeyFrame *tckf);

int DP_transient_camera_key_frame_value_index(DP_TransientCameraKeyFrame *tckf,
                                              int prop);

int DP_transient_camera_key_frame_prop_type_at(DP_TransientCameraKeyFrame *tckf,
                                               int index);

double DP_transient_camera_key_frame_value_at(DP_TransientCameraKeyFrame *tckf,
                                              int index);

int DP_transient_camera_key_frame_curve_index(DP_TransientCameraKeyFrame *tckf,
                                              int prop);

DP_Curve *
DP_transient_camera_key_frame_curve_at_noinc(DP_TransientCameraKeyFrame *tckf,
                                             int index);

void DP_transient_camera_key_frame_title_set(DP_TransientCameraKeyFrame *tckf,
                                             const char *title, size_t length);

void DP_transient_camera_key_frame_value_set_at(
    DP_TransientCameraKeyFrame *tckf, int index, int prop, double value);

void DP_transient_camera_key_frame_curve_set_at_noinc(
    DP_TransientCameraKeyFrame *tckf, int index, int prop, DP_Curve *curve);

void DP_transient_camera_key_frame_curve_set_at_inc(
    DP_TransientCameraKeyFrame *tckf, int index, int prop, DP_Curve *curve);

void DP_transient_camera_key_frame_delete_at(DP_TransientCameraKeyFrame *tckf,
                                             int index);

#endif
