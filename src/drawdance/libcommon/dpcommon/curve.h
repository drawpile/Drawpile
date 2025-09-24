// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPCOMMON_CUBIC_CURVE_H
#define DPCOMMON_CUBIC_CURVE_H

typedef struct DP_Vec2 DP_Vec2;


#define DP_CURVE_TYPE_LINEAR 0
#define DP_CURVE_TYPE_CUBIC  1

typedef struct DP_Curve DP_Curve;
typedef void (*DP_CurveGetPointFn)(void *user, int i, double *out_x,
                                   double *out_y);

DP_Curve *DP_curve_new(int type, int count, DP_CurveGetPointFn get_point,
                       void *user);

DP_Curve *DP_curve_incref(DP_Curve *curve);

DP_Curve *DP_curve_incref_nullable(DP_Curve *curve_or_null);

void DP_curve_decref(DP_Curve *curve);

void DP_curve_decref_nullable(DP_Curve *curve_or_null);

int DP_curve_refcount(DP_Curve *curve);

int DP_curve_type(DP_Curve *curve);

const DP_Vec2 *DP_curve_points(DP_Curve *curve, int *out_count);

int DP_curve_points_count(DP_Curve *curve);

double DP_curve_value_at(DP_Curve *curve, double x);


#endif
