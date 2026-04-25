// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_FILTER_PROPS_H
#define DPENGINE_FILTER_PROPS_H
#include <dpcommon/common.h>

DP_TYPEDEF_PERSISTENT(Tile);


// If you think of HSV/HSL/HSY as a vector, hue is the angle; saturation,
// chroma, lightness and luma is the magnitude. Hence this naming.
#define DP_FILTER_PROPS_ADJUST_ANGLE_MIN     (-1.0f)
#define DP_FILTER_PROPS_ADJUST_ANGLE_MAX     1.0f
#define DP_FILTER_PROPS_ADJUST_MAGNITUDE_MIN 0.0f
#define DP_FILTER_PROPS_ADJUST_MAGNITUDE_MAX 10.0f

typedef enum DP_FilterType {
    DP_FILTER_TYPE_UNKNOWN = 1,
    DP_FILTER_TYPE_HSV_ADJUST,
    DP_FILTER_TYPE_HSL_ADJUST,
    DP_FILTER_TYPE_HCY_ADJUST,
} DP_FilterType;

typedef struct DP_FilterProps DP_FilterProps;
typedef struct DP_FilterPropsUnknown DP_FilterPropsUnknown;
typedef struct DP_FilterPropsHsvAdjust DP_FilterPropsHsvAdjust;
typedef struct DP_FilterPropsHslAdjust DP_FilterPropsHslAdjust;
typedef struct DP_FilterPropsHcyAdjust DP_FilterPropsHcyAdjust;


DP_FilterProps *DP_filter_props_new_unknown(int actual_type,
                                            const unsigned char *data,
                                            size_t size);

DP_FilterProps *DP_filter_props_new_hsv_adjust(float h, float s, float v);
DP_FilterProps *DP_filter_props_new_hsl_adjust(float h, float s, float l);
DP_FilterProps *DP_filter_props_new_hcy_adjust(float h, float c, float y);


DP_FilterProps *DP_filter_props_incref(DP_FilterProps *fp);

DP_FilterProps *DP_filter_props_incref_nullable(DP_FilterProps *fp_or_null);

void DP_filter_props_decref(DP_FilterProps *fp);

void DP_filter_props_decref_nullable(DP_FilterProps *fp_or_null);

int DP_filter_props_refcount(DP_FilterProps *fp);

bool DP_filter_props_differ(DP_FilterProps *fp, DP_FilterProps *prev_fp);

bool DP_filter_props_differ_nullable(DP_FilterProps *fp_or_null,
                                     DP_FilterProps *prev_fp_or_null);


size_t DP_filter_props_serialize(DP_FilterProps *fp,
                                 unsigned char *(*get_buffer)(void *, size_t),
                                 void *user);

DP_FilterProps *DP_filter_props_deserialize(const unsigned char *data,
                                            size_t size);


int DP_filter_props_type(DP_FilterProps *fp);

void *DP_filter_props_internal(DP_FilterProps *fp);

bool DP_filter_props_effective(DP_FilterProps *fp);

void DP_filter_props_apply_tile(DP_FilterProps *fp, DP_TransientTile *tt,
                                DP_Tile *t);


float DP_filter_props_hsv_adjust_h(DP_FilterPropsHsvAdjust *fpha);
float DP_filter_props_hsv_adjust_s(DP_FilterPropsHsvAdjust *fpha);
float DP_filter_props_hsv_adjust_v(DP_FilterPropsHsvAdjust *fpha);

float DP_filter_props_hsl_adjust_h(DP_FilterPropsHslAdjust *fpha);
float DP_filter_props_hsl_adjust_s(DP_FilterPropsHslAdjust *fpha);
float DP_filter_props_hsl_adjust_l(DP_FilterPropsHslAdjust *fpha);

float DP_filter_props_hcy_adjust_h(DP_FilterPropsHcyAdjust *fpha);
float DP_filter_props_hcy_adjust_c(DP_FilterPropsHcyAdjust *fpha);
float DP_filter_props_hcy_adjust_y(DP_FilterPropsHcyAdjust *fpha);


#endif
