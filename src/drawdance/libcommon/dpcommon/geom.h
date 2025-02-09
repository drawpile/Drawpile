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
 * This code is wholly based on the Qt framework's geometry transform
 * implementation, using it under the GNU General Public License, version 3.
 * See 3rdparty/licenses/qt/license.GPL3 for details.
 */
#ifndef DPCOMMON_GEOM_H
#define DPCOMMON_GEOM_H
#include "common.h"
#include <math.h>


typedef struct DP_Quad {
    int x1, y1;
    int x2, y2;
    int x3, y3;
    int x4, y4;
} DP_Quad;

typedef struct DP_Rect {
    int x1, y1;
    int x2, y2;
} DP_Rect;

typedef struct DP_Vec2 {
    double x, y;
} DP_Vec2;

typedef struct DP_Transform {
    double matrix[9];
} DP_Transform;

typedef struct DP_MaybeTransform {
    DP_Transform tf;
    bool valid;
} DP_MaybeTransform;


DP_INLINE DP_Quad DP_quad_make(int x1, int y1, int x2, int y2, int x3, int y3,
                               int x4, int y4)
{
    DP_Quad quad = {x1, y1, x2, y2, x3, y3, x4, y4};
    return quad;
}

DP_INLINE int DP_quad_min(int a, int b, int c, int d)
{
    return DP_min_int(a, DP_min_int(b, DP_min_int(c, d)));
}

DP_INLINE int DP_quad_max(int a, int b, int c, int d)
{
    return DP_max_int(a, DP_max_int(b, DP_max_int(c, d)));
}

DP_INLINE DP_Rect DP_quad_bounds(DP_Quad quad)
{
    DP_Rect rect = {
        DP_quad_min(quad.x1, quad.x2, quad.x3, quad.x4),
        DP_quad_min(quad.y1, quad.y2, quad.y3, quad.y4),
        DP_quad_max(quad.x1, quad.x2, quad.x3, quad.x4),
        DP_quad_max(quad.y1, quad.y2, quad.y3, quad.y4),
    };
    return rect;
}

DP_INLINE DP_Quad DP_quad_translate(DP_Quad quad, int dx, int dy)
{
    return DP_quad_make(quad.x1 + dx, quad.y1 + dy, quad.x2 + dx, quad.y2 + dy,
                        quad.x3 + dx, quad.y3 + dy, quad.x4 + dx, quad.y4 + dy);
}


DP_INLINE DP_Rect DP_rect_make(int x, int y, int width, int height)
{
    DP_ASSERT(width >= 0);
    DP_ASSERT(height >= 0);
    int x2, y2;
    if (!DP_add_overflow_int(x, width - 1, &x2)
        && !DP_add_overflow_int(y, height - 1, &y2)) {
        DP_Rect rect = {x, y, x2, y2};
        return rect;
    }
    else {
        DP_Rect invalid_rect = {0, 0, -1, -1};
        return invalid_rect;
    }
}

DP_INLINE bool DP_rect_valid(DP_Rect rect)
{
    return rect.x1 <= rect.x2 && rect.y1 <= rect.y2;
}

DP_INLINE bool DP_rect_empty(DP_Rect rect)
{
    return rect.x1 >= rect.x2 && rect.y1 >= rect.y2;
}

DP_INLINE int DP_rect_x(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.x1;
}

DP_INLINE int DP_rect_y(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.y1;
}

DP_INLINE int DP_rect_left(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.x1;
}

DP_INLINE int DP_rect_top(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.y1;
}

DP_INLINE int DP_rect_right(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.x2;
}

DP_INLINE int DP_rect_bottom(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.y2;
}

DP_INLINE void DP_rect_sides(DP_Rect rect, int *out_left, int *out_top,
                             int *out_right, int *out_bottom)
{
    if (out_left) {
        *out_left = DP_rect_left(rect);
    }
    if (out_top) {
        *out_top = DP_rect_top(rect);
    }
    if (out_right) {
        *out_right = DP_rect_right(rect);
    }
    if (out_bottom) {
        *out_bottom = DP_rect_bottom(rect);
    }
}

DP_INLINE int DP_rect_width(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.x2 - rect.x1 + 1;
}

DP_INLINE int DP_rect_height(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    return rect.y2 - rect.y1 + 1;
}

DP_INLINE long long DP_rect_size(DP_Rect rect)
{
    DP_ASSERT(DP_rect_valid(rect));
    long long width = DP_rect_width(rect);
    long long height = DP_rect_height(rect);
    return width * height;
}

DP_INLINE DP_Rect DP_rect_translate(DP_Rect rect, int tx, int ty)
{
    DP_ASSERT(DP_rect_valid(rect));
    DP_Rect result;
    result.x1 = rect.x1 + tx;
    result.y1 = rect.y1 + ty;
    result.x2 = rect.x2 + tx;
    result.y2 = rect.y2 + ty;
    return result;
}

DP_INLINE DP_Rect DP_rect_union(DP_Rect a, DP_Rect b)
{
    DP_ASSERT(DP_rect_valid(a));
    DP_ASSERT(DP_rect_valid(b));
    DP_Rect result;
    result.x1 = DP_min_int(a.x1, b.x1);
    result.x2 = DP_max_int(a.x2, b.x2);
    result.y1 = DP_min_int(a.y1, b.y1);
    result.y2 = DP_max_int(a.y2, b.y2);
    return result;
}

DP_INLINE DP_Rect DP_rect_intersection(DP_Rect a, DP_Rect b)
{
    if (DP_rect_valid(a) && DP_rect_valid(b)) {
        DP_Rect result;
        result.x1 = DP_max_int(a.x1, b.x1);
        result.x2 = DP_min_int(a.x2, b.x2);
        result.y1 = DP_max_int(a.y1, b.y1);
        result.y2 = DP_min_int(a.y2, b.y2);
        return result;
    }
    else {
        DP_Rect result = {0, 0, -1, -1};
        return result;
    }
}

DP_INLINE bool DP_rect_intersects(DP_Rect a, DP_Rect b)
{
    return DP_rect_valid(a) && DP_rect_valid(b)
        && ((a.x1 <= b.x1 && b.x1 <= a.x2) || (b.x1 <= a.x1 && a.x1 <= b.x2))
        && ((a.y1 <= b.y1 && b.y1 <= a.y2) || (b.y1 <= a.y1 && a.y1 <= b.y2));
}

DP_INLINE bool DP_rect_contains(DP_Rect rect, int x, int y)
{
    return DP_rect_valid(rect) && x >= rect.x1 && x <= rect.x2 && y >= rect.y1
        && y <= rect.y2;
}


DP_INLINE DP_Transform DP_transform_make(double m11, double m12, double m13,
                                         double m21, double m22, double m23,
                                         double m31, double m32, double m33)
{
    DP_Transform tf = {{m11, m12, m13, m21, m22, m23, m31, m32, m33}};
    return tf;
}

DP_INLINE DP_Transform DP_transform_identity(void)
{
    return DP_transform_make(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
}

DP_INLINE DP_Transform DP_transform_translation(double x, double y)
{
    return DP_transform_make(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, x, y, 1.0);
}

DP_INLINE DP_Transform DP_transform_scaling(double scale_x, double scale_y)
{
    return DP_transform_make(scale_x, 0.0, 0.0, 0.0, scale_y, 0.0, 0.0, 0.0,
                             1.0);
}

DP_INLINE DP_Transform DP_transform_rotation(double rotation_in_radians)
{
    double c = cos(rotation_in_radians);
    double s = sin(rotation_in_radians);
    return DP_transform_make(c, s, 0.0, -s, c, 0.0, 0.0, 0.0, 1.0);
}

DP_INLINE DP_MaybeTransform DP_maybe_transform_from(DP_Transform tf)
{
    DP_MaybeTransform mtf = {tf, true};
    return mtf;
}

DP_INLINE DP_MaybeTransform DP_maybe_transform_make(double m11, double m12,
                                                    double m13, double m21,
                                                    double m22, double m23,
                                                    double m31, double m32,
                                                    double m33)
{
    return DP_maybe_transform_from(
        DP_transform_make(m11, m12, m13, m21, m22, m23, m31, m32, m33));
}

DP_INLINE DP_MaybeTransform DP_maybe_transform_invalid(void)
{
    DP_MaybeTransform mtf;
    mtf.valid = false;
    return mtf;
}

DP_INLINE DP_Transform DP_transform_mul(DP_Transform a, DP_Transform b)
{
    double *am = a.matrix;
    double *bm = b.matrix;
    return DP_transform_make(am[0] * bm[0] + am[1] * bm[3] + am[2] * bm[6],
                             am[0] * bm[1] + am[1] * bm[4] + am[2] * bm[7],
                             am[0] * bm[2] + am[1] * bm[5] + am[2] * bm[8],
                             am[3] * bm[0] + am[4] * bm[3] + am[5] * bm[6],
                             am[3] * bm[1] + am[4] * bm[4] + am[5] * bm[7],
                             am[3] * bm[2] + am[4] * bm[5] + am[5] * bm[8],
                             am[6] * bm[0] + am[7] * bm[3] + am[8] * bm[6],
                             am[6] * bm[1] + am[7] * bm[4] + am[8] * bm[7],
                             am[6] * bm[2] + am[7] * bm[5] + am[8] * bm[8]);
}

DP_INLINE DP_Transform DP_transform_translate(DP_Transform tf, double x,
                                              double y)
{
    return DP_transform_mul(tf, DP_transform_translation(x, y));
}

DP_INLINE DP_Transform DP_transform_scale(DP_Transform tf, double scale_x,
                                          double scale_y)
{
    return DP_transform_mul(tf, DP_transform_scaling(scale_x, scale_y));
}

DP_INLINE DP_Transform DP_transform_rotate(DP_Transform tf,
                                           double rotation_in_radians)
{
    return DP_transform_mul(tf, DP_transform_rotation(rotation_in_radians));
}

DP_INLINE DP_Vec2 DP_transform_xy(DP_Transform tf, double x, double y)
{
    double *m = tf.matrix;
    double hx = m[0] * x + m[3] * y + m[6];
    double hy = m[1] * x + m[4] * y + m[7];
    double hw = m[2] * x + m[5] * y + m[8];
    DP_Vec2 v;
    v.x = hx / hw;
    v.y = hy / hw;
    return v;
}

DP_INLINE double DP_transform_determinant(DP_Transform tf)
{
    double *m = tf.matrix;
    return (m[0] * (m[8] * m[4] - m[7] * m[5]))
         - (m[3] * (m[8] * m[1] - m[7] * m[2]))
         + (m[6] * (m[5] * m[1] - m[4] * m[2]));
}

DP_INLINE DP_MaybeTransform DP_transform_invert(DP_Transform tf)
{
    double det = DP_transform_determinant(tf);
    if (det < -0.000000000001 || det > 0.000000000001) {
        double *m = tf.matrix;
        return DP_maybe_transform_make((m[4] * m[8] - m[5] * m[7]) / det,
                                       (m[5] * m[6] - m[3] * m[8]) / det,
                                       (m[3] * m[7] - m[4] * m[6]) / det,
                                       (m[2] * m[7] - m[1] * m[8]) / det,
                                       (m[0] * m[8] - m[2] * m[6]) / det,
                                       (m[1] * m[6] - m[0] * m[7]) / det,
                                       (m[1] * m[5] - m[2] * m[4]) / det,
                                       (m[2] * m[3] - m[0] * m[5]) / det,
                                       (m[0] * m[4] - m[1] * m[3]) / det);
    }
    else {
        return DP_maybe_transform_invalid();
    }
}

DP_INLINE DP_Transform DP_transform_transpose(DP_Transform tf)
{
    double *m = tf.matrix;
    return DP_transform_make(m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5],
                             m[8]);
}

DP_INLINE DP_MaybeTransform DP_transform_unit_square_to_quad(DP_Quad quad)
{
    double x1 = quad.x1, y1 = quad.y1;
    double x2 = quad.x2, y2 = quad.y2;
    double x3 = quad.x3, y3 = quad.y3;
    double x4 = quad.x4, y4 = quad.y4;
    double ax = x1 - x2 + x3 - x4;
    double ay = y1 - y2 + y3 - y4;
    if (ax == 0.0 && ay == 0.0) {
        return DP_maybe_transform_make(x2 - x1, y2 - y1, 0.0, x3 - x2, y3 - y2,
                                       0.0, x1, y1, 1.0);
    }
    else {
        double ax1 = x2 - x3;
        double ax2 = x4 - x3;
        double ay1 = y2 - y3;
        double ay2 = y4 - y3;
        double bot = ax1 * ay2 - ax2 * ay1;
        if (bot != 0.0) {
            double m13 = (ax * ay2 - ax2 * ay) / bot;
            double m11 = x2 - x1 + m13 * x2;
            double m12 = y2 - y1 + m13 * y2;
            double m23 = (ax1 * ay - ax * ay1) / bot;
            double m21 = x4 - x1 + m23 * x4;
            double m22 = y4 - y1 + m23 * y4;
            return DP_maybe_transform_make(m11, m12, m13, m21, m22, m23, x1, y1,
                                           1.0);
        }
        else {
            return DP_maybe_transform_invalid();
        }
    }
}

DP_INLINE DP_MaybeTransform DP_transform_quad_to_unit_square(DP_Quad quad)
{
    DP_MaybeTransform stq = DP_transform_unit_square_to_quad(quad);
    if (stq.valid) {
        return DP_transform_invert(stq.tf);
    }
    else {
        return stq;
    }
}

DP_INLINE DP_MaybeTransform DP_transform_quad_to_quad(DP_Quad src_quad,
                                                      DP_Quad dst_quad)
{
    DP_MaybeTransform qts = DP_transform_quad_to_unit_square(src_quad);
    if (qts.valid) {
        DP_MaybeTransform stq = DP_transform_unit_square_to_quad(dst_quad);
        if (stq.valid) {
            return DP_maybe_transform_from(DP_transform_mul(qts.tf, stq.tf));
        }
    }
    return DP_maybe_transform_invalid();
}


#endif
