extern "C" {
#include "stable_sort.h"
#include "geom.h"
}
#include <algorithm>


static bool compare_curve_points(const DP_Vec2 &a, const DP_Vec2 &b)
{
    return a.x < b.x;
}

extern "C" void DP_stable_sort_curve_points(DP_Vec2 *points, int count)
{
    std::stable_sort(points, points + count, compare_curve_points);
}
