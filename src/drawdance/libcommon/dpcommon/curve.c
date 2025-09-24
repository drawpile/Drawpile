// SPDX-License-Identifier: GPL-3.0-or-later
#include "curve.h"
#include "atomic.h"
#include "common.h"
#include "conversions.h"
#include "geom.h"
#include "stable_sort.h"


typedef int (*DP_CurveGetCountFn)(void *curve);
typedef const DP_Vec2 *(*DP_CurveGetPointsFn)(void *curve);
typedef double (*DP_CurveGetValueAtFn)(void *curve, double x);

struct DP_Curve {
    DP_Atomic refcount;
    int type;
    DP_CurveGetCountFn get_count;
    DP_CurveGetPointsFn get_points;
    DP_CurveGetValueAtFn get_value_at;
};

static DP_Curve *curve_init(DP_Curve *curve, DP_CurveGetCountFn get_count,
                            DP_CurveGetPointsFn get_points,
                            DP_CurveGetValueAtFn get_value_at)

{
    DP_ASSERT(curve);
    DP_ASSERT(get_count);
    DP_ASSERT(get_points);
    DP_ASSERT(get_value_at);
    curve->get_count = get_count;
    curve->get_points = get_points;
    curve->get_value_at = get_value_at;
    return curve;
}

static DP_Vec2 curve_get_point(int i, DP_CurveGetPointFn get_point, void *user)
{
    DP_Vec2 point;
    get_point(user, i, &point.x, &point.y);
    return point;
}

static void curve_copy_points(int count, DP_Vec2 *dst,
                              DP_CurveGetPointFn get_point, void *user)
{
    for (int i = 0; i < count; ++i) {
        dst[i] = curve_get_point(i, get_point, user);
    }
}

static bool curve_value_inside(double x, DP_Vec2 first, DP_Vec2 last,
                               double *out_y)
{
    if (x <= first.x) {
        *out_y = first.y;
        return false;
    }
    else if (x >= last.x) {
        *out_y = last.y;
        return false;
    }
    else {
        return true;
    }
}

static double curve_lerp(double x, DP_Vec2 a, DP_Vec2 b)
{
    DP_ASSERT(a.x <= x);
    DP_ASSERT(b.x >= x);
    double t = (x - a.x) / (b.x - a.x);
    return DP_lerp_double(a.y, b.y, t);
}


static int null_curve_get_count(DP_UNUSED void *curve)
{
    return 0;
}

static const DP_Vec2 *null_curve_get_points(DP_UNUSED void *curve)
{
    return NULL;
}

static double null_curve_get_value_at(DP_UNUSED void *curve, double x)
{
    return x;
}

static DP_Curve *null_curve_new(void)
{
    DP_Curve *curve = DP_malloc(sizeof(*curve));
    return curve_init(curve, null_curve_get_count, null_curve_get_points,
                      null_curve_get_value_at);
}


typedef struct DP_MonoCurve {
    DP_Curve parent;
    DP_Vec2 point;
} DP_MonoCurve;

static int mono_curve_get_count(DP_UNUSED void *curve)
{
    return 1;
}

static const DP_Vec2 *mono_curve_get_points(void *curve)
{
    DP_MonoCurve *mc = curve;
    return &mc->point;
}

static double mono_curve_get_value_at(void *curve, DP_UNUSED double x)
{
    DP_MonoCurve *mc = curve;
    return mc->point.y;
}

static DP_Curve *mono_curve_new(DP_Vec2 point)
{
    DP_MonoCurve *mc = DP_malloc(sizeof(*mc));
    mc->point = point;
    return curve_init(&mc->parent, mono_curve_get_count, mono_curve_get_points,
                      mono_curve_get_value_at);
}


typedef struct DP_DuoCurve {
    DP_Curve parent;
    DP_Vec2 points[2];
} DP_DuoCurve;

static int duo_curve_get_count(DP_UNUSED void *curve)
{
    return 2;
}

static const DP_Vec2 *duo_curve_get_points(void *curve)
{
    DP_DuoCurve *dc = curve;
    return dc->points;
}

static double duo_curve_get_value_at(void *curve, double x)
{
    DP_DuoCurve *dc = curve;
    DP_Vec2 a = dc->points[0];
    DP_Vec2 b = dc->points[1];
    double y;
    if (curve_value_inside(x, a, b, &y)) {
        y = curve_lerp(x, a, b);
    }
    return y;
}

static DP_Curve *duo_curve_new(DP_Vec2 a, DP_Vec2 b)
{
    DP_DuoCurve *dc = DP_malloc(sizeof(*dc));
    if (a.x <= b.x) {
        dc->points[0] = a;
        dc->points[1] = b;
    }
    else {
        dc->points[0] = b;
        dc->points[1] = a;
    }
    return curve_init(&dc->parent, duo_curve_get_count, duo_curve_get_points,
                      duo_curve_get_value_at);
}


typedef struct DP_LinearCurve {
    DP_Curve parent;
    int count;
    DP_Vec2 points[];
} DP_LinearCurve;

static int linear_curve_get_count(void *curve)
{
    DP_LinearCurve *lc = curve;
    return lc->count;
}

static const DP_Vec2 *linear_curve_get_points(void *curve)
{
    DP_LinearCurve *lc = curve;
    return lc->points;
}

static double linear_curve_get_value_at(void *curve, double x)
{
    DP_LinearCurve *lc = curve;
    int last = lc->count - 1;
    double y;
    if (curve_value_inside(x, lc->points[0], lc->points[last], &y)) {
        int i = 0;
        while (x < lc->points[i].x) {
            ++i;
        }
        DP_ASSERT(i < last);
        y = curve_lerp(x, lc->points[i], lc->points[i + 1]);
    }
    return y;
}

static DP_Curve *linear_curve_new(int count, DP_CurveGetPointFn get_point,
                                  void *user)
{
    DP_ASSERT(get_point);
    DP_ASSERT(count > 1);
    size_t scount = DP_int_to_size(count);
    DP_LinearCurve *lc =
        DP_malloc(DP_FLEX_SIZEOF(DP_LinearCurve, points, scount));
    lc->count = count;
    curve_copy_points(count, lc->points, get_point, user);
    DP_stable_sort_curve_points(lc->points, count);
    return curve_init(&lc->parent, linear_curve_get_count,
                      linear_curve_get_points, linear_curve_get_value_at);
}


static_assert(alignof(DP_Vec2) == alignof(double),
              "DP_Vec2 has double alignment");

typedef struct DP_CubicCurve {
    DP_Curve parent;
    int count;
    alignas(double) unsigned char data[];
} DP_CubicCurve;

static DP_Vec2 *cubic_curve_points(DP_CubicCurve *cc)
{
    // Detour via void * because of bogus alignment warning.
    return (void *)cc->data;
}

static double *cubic_curve_a(DP_CubicCurve *cc)
{
    return (double *)(cubic_curve_points(cc) + cc->count);
}

static double *cubic_curve_h(DP_CubicCurve *cc)
{
    return cubic_curve_a(cc) + cc->count;
}

static double *cubic_curve_c(DP_CubicCurve *cc)
{
    return cubic_curve_a(cc) + (cc->count * 2 - 1);
}

static double *cubic_curve_d(DP_CubicCurve *cc)
{
    return cubic_curve_a(cc) + (cc->count * 3 - 1);
}

static double *cubic_curve_b(DP_CubicCurve *cc)
{
    return cubic_curve_a(cc) + (cc->count * 4 - 2);
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-SnippetCopyrightText: Copyright (c) 2005 C. Boemann
// SPDX-SnippetCopyrightText: Copyright (c) 2009 Dmitry Kazakov
// SPDX-SnippetCopyrightText: Copyright (c) 2010 Cyrille Berger
// SPDX-SnippetCopyrightText: Copyright (c) 2020 Björn Ottosson
// SDPX—SnippetName: KisCubicCurve spline calculations from Krita

static bool cubic_curve_create_spline(DP_CubicCurve *cc)
{
#define CUBIC_CURVE_SET_DO(DST, SRC, EXTRA_STMT) \
    do {                                         \
        double _value = (SRC);                   \
        if (isfinite(_value)) {                  \
            (DST) = _value;                      \
        }                                        \
        else {                                   \
            EXTRA_STMT;                          \
            return false;                        \
        }                                        \
    } while (0)
#define CUBIC_CURVE_SET(DST, SRC)   CUBIC_CURVE_SET_DO(DST, SRC, )
#define CUBIC_CURVE_SET_A(DST, SRC) CUBIC_CURVE_SET_DO(DST, SRC, DP_free(tri_a))

    int count = cc->count;
    int intervals = count - 1;
    int tri_count = count - 2;
    DP_ASSERT(count >= 3);

    DP_Vec2 *a = cubic_curve_points(cc);
    double *DP_RESTRICT m_a = cubic_curve_a(cc);
    double *DP_RESTRICT m_h = cubic_curve_h(cc);
    for (int i = 0; i < intervals; ++i) {
        CUBIC_CURVE_SET(m_h[i], a[i + 1].x - a[i].x);
        CUBIC_CURVE_SET(m_a[i], a[i].y);
    }
    CUBIC_CURVE_SET(m_a[intervals], a[intervals].y);

    // Since the memory is already allocated, tri_f = m_d, tri_b = m_b.
    double *DP_RESTRICT m_d = cubic_curve_d(cc);
    double *DP_RESTRICT m_b = cubic_curve_b(cc);

    // One temporary buffer, holds tri_b[tri_count - 1] + alpha[tri_count] +
    // beta[tri_count]. If tri_count is 1, then alpha and beta aren't needed.
    size_t stri_count = DP_int_to_size(tri_count);
    double *DP_RESTRICT tri_a = DP_malloc(
        sizeof(*tri_a) * (stri_count * (tri_count == 1 ? (size_t)1 : (size_t)3))
        - (size_t)1);

    for (int i = 0; i < intervals - 1; ++i) {
        m_b[i] = 2.0 * (m_h[i] + m_h[i + 1]);
        m_d[i] = 6.0
               * ((m_a[i + 2] - m_a[i + 1]) / m_h[i + 1]
                  - (m_a[i + 1] - m_a[i]) / m_h[i]);
    }

    for (int i = 1; i < intervals - 1; ++i) {
        tri_a[i - 1] = m_h[i];
    }

    double *DP_RESTRICT m_c = cubic_curve_c(cc);
    m_c[0] = 0.0;
    if (tri_count == 1) {
        CUBIC_CURVE_SET_A(m_c[1], m_d[0] / m_b[0]);
    }
    else {
        double *DP_RESTRICT alpha = tri_a + (stri_count - (size_t)1);
        double *DP_RESTRICT beta = alpha + stri_count;

        alpha[0] = -tri_a[0] / m_b[0];
        beta[0] = m_d[0] / m_b[0];

        for (int i = 1; i < tri_count - 1; i++) {
            alpha[i] = -tri_a[i] / ((tri_a[i - 1] * alpha[i - 1]) + m_b[i]);
            beta[i] = (m_d[i] - (tri_a[i - 1] * beta[i - 1]))
                    / ((tri_a[i - 1] * alpha[i - 1]) + m_b[i]);
        }

        CUBIC_CURVE_SET_A(
            m_c[tri_count],
            (m_d[tri_count - 1] - (tri_a[tri_count - 2] * beta[tri_count - 2]))
                / (m_b[tri_count - 1]
                   + (tri_a[tri_count - 2] * alpha[tri_count - 2])));

        for (int i = tri_count - 2; i >= 0; --i) {
            CUBIC_CURVE_SET_A(m_c[i + 1], alpha[i] * m_c[i + 2] + beta[i]);
        }
    }
    m_c[intervals] = 0.0;

    DP_free(tri_a);

    for (int i = 0; i < intervals; ++i) {
        CUBIC_CURVE_SET(m_d[i], (m_c[i + 1] - m_c[i]) / m_h[i]);
    }

    for (int i = 0; i < intervals; ++i) {
        CUBIC_CURVE_SET(m_b[i], -0.5 * (m_c[i] * m_h[i])
                                    - (1 / 6.0) * (m_d[i] * m_h[i] * m_h[i])
                                    + (m_a[i + 1] - m_a[i]) / m_h[i]);
    }

    return true;
}

static int cubic_curve_spline_find_region(DP_CubicCurve *cc, double x,
                                          double *out_x0)
{
    int intervals = cc->count - 1;
    const double *m_h = cubic_curve_h(cc);
    double x0 = cubic_curve_points(cc)[0].x;

    for (int i = 0; i < intervals; ++i) {
        if (x >= x0 && x < x0 + m_h[i]) {
            *out_x0 = x0;
            return i;
        }
        x0 += m_h[i];
    }

    if (x >= x0) {
        x0 -= m_h[intervals - 1];
        *out_x0 = x0;
        return intervals - 1;
    }

    DP_UNREACHABLE();
}

static double cubic_curve_spline_get_value(DP_CubicCurve *cc, double x)
{
    double x0;
    int i = cubic_curve_spline_find_region(cc, x, &x0);
    double xd = x - x0;
    return cubic_curve_a(cc)[i] + cubic_curve_b(cc)[i] * xd
         + 0.5 * cubic_curve_c(cc)[i] * xd * xd
         + (1.0 / 6.0) * cubic_curve_d(cc)[i] * xd * xd * xd;
}

// SDPX—SnippetEnd

static int cubic_curve_get_count(void *curve)
{
    DP_CubicCurve *cc = curve;
    return cc->count;
}

static const DP_Vec2 *cubic_curve_get_points(void *curve)
{
    DP_CubicCurve *cc = curve;
    return cubic_curve_points(cc);
}

static double cubic_curve_get_value_at(void *curve, double x)
{
    DP_CubicCurve *cc = curve;
    DP_Vec2 *points = cubic_curve_points(cc);
    int last = cc->count - 1;
    double y;
    if (curve_value_inside(x, points[0], points[last], &y)) {
        y = cubic_curve_spline_get_value(cc, x);
    }
    return y;
}

static double cubic_curve_get_value_at_invalid(DP_UNUSED void *curve,
                                               DP_UNUSED double x)
{
    return 0.0; // Invalid KisCubicCurve also always returns 0.
}

static DP_Curve *cubic_curve_new(int count, DP_CurveGetPointFn get_point,
                                 void *user)
{
    DP_ASSERT(get_point);
    DP_ASSERT(count >= 3);
    size_t scount = DP_int_to_size(count);
    // Data holds points[count] + m_a[count] + m_h[count - 1] + m_c[count] +
    // m_d[count - 1] + m_b[count - 1].
    size_t data_size = scount * sizeof(DP_Vec2)
                     + (sizeof(double) * ((scount * (size_t)5) - (size_t)3));
    DP_CubicCurve *cc =
        DP_malloc(DP_FLEX_SIZEOF(DP_CubicCurve, data, data_size));
    cc->count = count;
    curve_copy_points(count, cubic_curve_points(cc), get_point, user);
    DP_stable_sort_curve_points(cubic_curve_points(cc), count);
    bool spline_ok = cubic_curve_create_spline(cc);
    return curve_init(&cc->parent, cubic_curve_get_count,
                      cubic_curve_get_points,
                      spline_ok ? cubic_curve_get_value_at
                                : cubic_curve_get_value_at_invalid);
}


DP_Curve *DP_curve_new(int type, int count, DP_CurveGetPointFn get_point,
                       void *user)
{
    DP_Curve *curve;
    if (count <= 0) {
        curve = null_curve_new();
    }
    else if (count == 1) {
        curve = mono_curve_new(curve_get_point(0, get_point, user));
    }
    else if (count == 2) {
        curve = duo_curve_new(curve_get_point(0, get_point, user),
                              curve_get_point(1, get_point, user));
    }
    else if (type == DP_CURVE_TYPE_CUBIC) {
        curve = cubic_curve_new(count, get_point, user);
    }
    else {
        curve = linear_curve_new(count, get_point, user);
    }

    DP_ASSERT(curve);
    DP_ASSERT(curve->get_count);
    DP_ASSERT(curve->get_points);
    DP_ASSERT(curve->get_value_at);

    DP_atomic_set(&curve->refcount, 1);
    curve->type = type;
    return curve;
}

DP_Curve *DP_curve_incref(DP_Curve *curve)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    DP_atomic_inc(&curve->refcount);
    return curve;
}

DP_Curve *DP_curve_incref_nullable(DP_Curve *curve_or_null)
{
    return curve_or_null ? DP_curve_incref(curve_or_null) : NULL;
}

void DP_curve_decref(DP_Curve *curve)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    if (DP_atomic_dec(&curve->refcount)) {
        DP_free(curve);
    }
}

void DP_curve_decref_nullable(DP_Curve *curve_or_null)
{
    if (curve_or_null) {
        DP_curve_decref(curve_or_null);
    }
}

int DP_curve_refcount(DP_Curve *curve)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    return DP_atomic_get(&curve->refcount);
}

int DP_curve_type(DP_Curve *curve)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    return curve->type;
}

const DP_Vec2 *DP_curve_points(DP_Curve *curve, int *out_count)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    if (out_count) {
        *out_count = curve->get_count(curve);
    }
    return curve->get_points(curve);
}

int DP_curve_points_count(DP_Curve *curve)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    return curve->get_count(curve);
}

double DP_curve_value_at(DP_Curve *curve, double x)
{
    DP_ASSERT(curve);
    DP_ASSERT(DP_atomic_get(&curve->refcount) > 0);
    double y = curve->get_value_at(curve, x);
    return DP_clamp_double(y, 0.0, 1.0);
}
