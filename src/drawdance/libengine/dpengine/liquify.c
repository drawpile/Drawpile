// SPDX-License-Identifier: GPL-3.0-or-later
#include "liquify.h"
#include "draw_context.h"
#include "image_transform.h"
#include "pixels.h"
#include "tile.h"
#include "tile_iterator.h"
#include <dpcommon/atomic.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/threading.h>
#include <math.h>
#if DP_LIQUIFY_DEBUG_OVERLAY
#    include <helpers.h> // For RGB <> HSV functions.
#endif


typedef struct DP_LiquifyTile DP_LiquifyTile;

// Tiles are kept in a freelist to be re-used.
struct DP_LiquifyTile {
    DP_ALIGNAS_SIMD float d[DP_TILE_LENGTH * 2];
    union {
        int refcount;
        DP_LiquifyTile *next_free;
    };
};

typedef struct DP_LiquifyMap {
    int refcount;
    DP_Rect tile_bounds;
    DP_LiquifyTile *tiles[];
} DP_LiquifyMap;

struct DP_Liquify {
    DP_Mutex *mutex;
    DP_LiquifyMap *lm;
    DP_LiquifyTile *free_tiles;
    DP_Atomic refcount;
    DP_Rect mask_rect;
};

struct DP_LiquifyState {
    DP_Liquify *l;
    DP_LiquifyMap *lm;
    DP_Atomic refcount;
};

typedef struct DP_LiquifyImage {
    uint32_t *data;
    int x;
    int y;
    int width;
    int height;
} DP_LiquifyImage;

struct DP_LiquifyTransformer {
    DP_LiquifyState *ls;
    DP_LiquifyImage source;
    DP_LiquifyImage target;
#if DP_LIQUIFY_DEBUG_OVERLAY
    DP_LiquifyImage debug;
#endif
    int interpolation;
    DP_Atomic refcount;
};


typedef void (*DP_LiquifyOpFn)(const DP_LiquifyOpParams *params,
                               DP_LiquifyMap *lm_or_null, int x, int y,
                               float alpha, float *out_x, float *out_y);


static DP_LiquifyTile *tile_alloc(DP_Liquify *l)
{
    DP_LiquifyTile *lt = l->free_tiles;
    if (lt) {
        l->free_tiles = lt->next_free;
    }
    else {
        lt = DP_malloc_simd(sizeof(*lt));
    }
    lt->refcount = 1;
    return lt;
}

static DP_LiquifyTile *tile_alloc_zeroed(DP_Liquify *l)
{
    DP_LiquifyTile *lt = l->free_tiles;
    if (lt) {
        l->free_tiles = lt->next_free;
        memset(lt->d, 0, sizeof(lt->d));
    }
    else {
        lt = DP_malloc_simd_zeroed(sizeof(*lt));
    }
    lt->refcount = 1;
    return lt;
}

static DP_LiquifyTile *tile_incref(DP_LiquifyTile *lt)
{
    DP_ASSERT(lt->refcount > 0);
    ++lt->refcount;
    return lt;
}

static DP_LiquifyTile *tile_incref_nullable(DP_LiquifyTile *lt_or_null)
{
    if (lt_or_null) {
        return tile_incref(lt_or_null);
    }
    else {
        return NULL;
    }
}

static void tile_decref(DP_Liquify *l, DP_LiquifyTile *lt)
{
    DP_ASSERT(lt->refcount > 0);
    if (--lt->refcount == 0) {
        lt->next_free = l->free_tiles;
        l->free_tiles = lt;
    }
}

static void tile_decref_nullable(DP_Liquify *l, DP_LiquifyTile *lt_or_null)
{
    if (lt_or_null) {
        tile_decref(l, lt_or_null);
    }
}

static DP_LiquifyTile *tile_make_editable(DP_Liquify *l, DP_LiquifyTile *lt)
{
    DP_ASSERT(lt->refcount > 0);
    // If nobody else is holding this tile, we can use it directly.
    if (lt->refcount == 1) {
        return lt;
    }
    else {
        DP_LiquifyTile *lt_copy = tile_alloc(l);
        memcpy(lt_copy->d, lt->d, sizeof(lt->d));
        DP_ASSERT(lt_copy->refcount == 1);
        return lt_copy;
    }
}

static DP_LiquifyTile *tile_make_editable_nullable(DP_Liquify *l,
                                                   DP_LiquifyTile *lt_or_null)
{
    if (lt_or_null) {
        return tile_make_editable(l, lt_or_null);
    }
    else {
        return tile_alloc_zeroed(l);
    }
}

static float *tile_xs(DP_LiquifyTile *lt)
{
    return lt->d;
}

static float *tile_ys(DP_LiquifyTile *lt)
{
    return lt->d + DP_TILE_LENGTH;
}

static int tile_index(int col, int row)
{
    DP_ASSERT(col >= 0);
    DP_ASSERT(col < DP_TILE_SIZE);
    DP_ASSERT(row >= 0);
    DP_ASSERT(row < DP_TILE_SIZE);
    return row * DP_TILE_SIZE + col;
}

static void tile_get_xy(DP_LiquifyTile *lt, int col, int row, float *out_x,
                        float *out_y)
{
    int i = tile_index(col, row);
    *out_x = tile_xs(lt)[i];
    *out_y = tile_ys(lt)[i];
}

static void tile_set_xy(DP_LiquifyTile *lt, int col, int row, float x, float y)
{
    int i = tile_index(col, row);
    tile_xs(lt)[i] = x;
    tile_ys(lt)[i] = y;
}

static bool tile_is_blank(DP_LiquifyTile *lt)
{
    static const float blank[DP_TILE_LENGTH * 2] = {0};
    static_assert(sizeof(blank) == sizeof(lt->d), "Liquify lengths match");
    return memcmp(lt->d, blank, sizeof(blank)) == 0;
}


static size_t map_tile_count(DP_Rect tile_bounds)
{
    return DP_int_to_size(DP_rect_width(tile_bounds))
         * DP_int_to_size(DP_rect_height(tile_bounds));
}

static DP_LiquifyMap *map_alloc(DP_Rect tile_bounds)
{
    DP_ASSERT(DP_rect_valid(tile_bounds));
    size_t count = map_tile_count(tile_bounds);
    DP_LiquifyMap *map = DP_malloc(DP_FLEX_SIZEOF(DP_LiquifyMap, tiles, count));
    map->refcount = 1;
    map->tile_bounds = tile_bounds;
    return map;
}

static DP_LiquifyMap *map_alloc_zeroed(DP_Rect tile_bounds)
{
    DP_ASSERT(DP_rect_valid(tile_bounds));
    size_t count = map_tile_count(tile_bounds);
    DP_LiquifyMap *map =
        DP_malloc_zeroed(DP_FLEX_SIZEOF(DP_LiquifyMap, tiles, count));
    map->refcount = 1;
    map->tile_bounds = tile_bounds;
    return map;
}

static DP_LiquifyMap *map_incref(DP_LiquifyMap *lm)
{
    DP_ASSERT(lm->refcount > 0);
    ++lm->refcount;
    return lm;
}

static DP_LiquifyMap *map_incref_nullable(DP_LiquifyMap *lm_or_null)
{
    if (lm_or_null) {
        return map_incref(lm_or_null);
    }
    else {
        return NULL;
    }
}

static void map_decref(DP_Liquify *l, DP_LiquifyMap *lm)
{
    DP_ASSERT(lm->refcount > 0);
    if (--lm->refcount == 0) {
        size_t count = map_tile_count(lm->tile_bounds);
        for (size_t i = 0; i < count; ++i) {
            tile_decref_nullable(l, lm->tiles[i]);
        }
        DP_free(lm);
    }
}

static void map_decref_nullable(DP_Liquify *l, DP_LiquifyMap *lm_or_null)
{
    if (lm_or_null) {
        map_decref(l, lm_or_null);
    }
}

static DP_Rect map_pixel_bounds(DP_LiquifyMap *lm)
{
    return DP_rect_make(DP_rect_x(lm->tile_bounds) * DP_TILE_SIZE,
                        DP_rect_y(lm->tile_bounds) * DP_TILE_SIZE,
                        DP_rect_width(lm->tile_bounds) * DP_TILE_SIZE,
                        DP_rect_height(lm->tile_bounds) * DP_TILE_SIZE);
}

static DP_LiquifyTile **map_tile(DP_LiquifyMap *lm, int tx, int ty)
{
    DP_ASSERT(tx >= lm->tile_bounds.x1);
    DP_ASSERT(tx <= lm->tile_bounds.x2);
    DP_ASSERT(ty >= lm->tile_bounds.y1);
    DP_ASSERT(ty <= lm->tile_bounds.y2);
    int mx = tx - lm->tile_bounds.x1;
    int my = ty - lm->tile_bounds.y1;
    return &lm->tiles[my * DP_rect_width(lm->tile_bounds) + mx];
}

static DP_LiquifyTile *map_get_tile(DP_LiquifyMap *lm, int tx, int ty)
{
    return *map_tile(lm, tx, ty);
}

static DP_LiquifyTile *map_get_tile_checked(DP_LiquifyMap *lm, int tx, int ty)
{
    if (DP_rect_contains(lm->tile_bounds, tx, ty)) {
        return *map_tile(lm, tx, ty);
    }
    else {
        return NULL;
    }
}

static void map_get_pixel(DP_LiquifyMap *lm, int x, int y, float *out_dx,
                          float *out_dy)
{
    int tx = DP_tile_coord_from_pixel(x);
    int ty = DP_tile_coord_from_pixel(y);
    DP_LiquifyTile *lt = map_get_tile_checked(lm, tx, ty);
    if (lt) {
        int x_in_tile = x - (tx * DP_TILE_SIZE);
        int y_in_tile = y - (ty * DP_TILE_SIZE);
        tile_get_xy(lt, x_in_tile, y_in_tile, out_dx, out_dy);
    }
    else {
        *out_dx = 0.0f;
        *out_dy = 0.0f;
    }
}

static void map_get_pixel_nullable(DP_LiquifyMap *lm_or_null, int x, int y,
                                   float *out_dx, float *out_dy)
{
    if (lm_or_null) {
        map_get_pixel(lm_or_null, x, y, out_dx, out_dy);
    }
    else {
        *out_dx = 0.0f;
        *out_dy = 0.0f;
    }
}

static void map_sample_pixel_bilinear(DP_LiquifyMap *lm, float xf, float yf,
                                      float *out_dx, float *out_dy)
{
    float xf0 = ceilf(xf - 0.5f);
    float yf0 = ceilf(yf - 0.5f);
    float xf1 = floorf(xf + 0.5f);
    float yf1 = floorf(yf + 0.5f);

    int x0 = DP_float_to_int(xf0);
    int y0 = DP_float_to_int(yf0);
    int x1 = DP_float_to_int(xf1);
    int y1 = DP_float_to_int(yf1);

    float dx00, dy00, dx01, dy01, dx10, dy10, dx11, dy11;
    map_get_pixel(lm, x0, y0, &dx00, &dy00);
    map_get_pixel(lm, x0, y1, &dx01, &dy01);
    map_get_pixel(lm, x1, y0, &dx10, &dy10);
    map_get_pixel(lm, x1, y1, &dx11, &dy11);

    float tx = xf - xf0;
    float ty = yf - yf0;

    *out_dx = (1.0f - tx) * (1.0f - ty) * dx00 + tx * (1.0f - ty) * dx10
            + (1.0f - tx) * ty * dx01 + tx * ty * dx11;
    *out_dy = (1.0f - tx) * (1.0f - ty) * dy00 + tx * (1.0f - ty) * dy10
            + (1.0f - tx) * ty * dy01 + tx * ty * dy11;
}

static void map_init_tile_inc(DP_LiquifyMap *lm, int tx, int ty,
                              DP_LiquifyTile *lt)
{
    DP_LiquifyTile **plt = map_tile(lm, tx, ty);
    DP_ASSERT(!*plt);
    *plt = tile_incref(lt);
}

static void map_set_tile_noinc(DP_Liquify *l, DP_LiquifyMap *lm, int tx, int ty,
                               DP_LiquifyTile *lt)
{
    DP_LiquifyTile **plt = map_tile(lm, tx, ty);
    if (*plt != lt) {
        tile_decref_nullable(l, *plt);
        *plt = lt;
    }
}

static void map_clear_tile(DP_Liquify *l, DP_LiquifyMap *lm, int tx, int ty,
                           DP_LiquifyTile *lt)
{
    DP_LiquifyTile **plt = map_tile(lm, tx, ty);
    if (*plt != lt) {
        tile_decref_nullable(l, *plt);
    }
    tile_decref(l, lt);
    *plt = NULL;
}

static DP_LiquifyMap *map_make_editable(DP_LiquifyMap *lm, DP_Rect tile_bounds)
{
    DP_ASSERT(lm->refcount > 0);
    DP_Rect old_tile_bounds = lm->tile_bounds;
    if (DP_rect_equal(tile_bounds, old_tile_bounds)) {
        // Same bounds, straight copy.
        if (lm->refcount == 1) {
            // If nobody else is holding this map, we can use it directly.
            return lm;
        }
        else {
            DP_LiquifyMap *lm_copy = map_alloc(tile_bounds);
            size_t count = map_tile_count(tile_bounds);
            for (size_t i = 0; i < count; ++i) {
                lm_copy->tiles[i] = tile_incref_nullable(lm->tiles[i]);
            }
            DP_ASSERT(lm_copy->refcount == 1);
            return lm_copy;
        }
    }
    else {
        // Different bounds, have to do some shifting.
        DP_LiquifyMap *lm_new = map_alloc_zeroed(tile_bounds);
        DP_Rect ir = DP_rect_intersection(tile_bounds, old_tile_bounds);
        for (int ty = ir.y1; ty <= ir.y2; ++ty) {
            for (int tx = ir.x1; tx <= ir.x2; ++tx) {
                DP_LiquifyTile *lt = map_get_tile(lm, tx, ty);
                if (lt) {
                    map_init_tile_inc(lm_new, tx, ty, lt);
                }
            }
        }
        DP_ASSERT(lm_new->refcount == 1);
        return lm_new;
    }
}


DP_Liquify *DP_liquify_new(int mask_x, int mask_y, int mask_width,
                           int mask_height)
{
    DP_ASSERT(mask_width > 0);
    DP_ASSERT(mask_height > 0);

    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return NULL;
    }

    DP_Liquify *l = DP_malloc(sizeof(*l));
    l->mutex = mutex;
    l->lm = NULL;
    l->free_tiles = NULL;
    DP_atomic_set(&l->refcount, 1);
    l->mask_rect = DP_rect_make(mask_x, mask_y, mask_width, mask_height);
    return l;
}

DP_Liquify *DP_liquify_incref(DP_Liquify *l)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);
    DP_atomic_inc(&l->refcount);
    return l;
}

DP_Liquify *DP_liquify_incref_nullable(DP_Liquify *l_or_null)
{
    if (l_or_null) {
        return DP_liquify_incref(l_or_null);
    }
    else {
        return NULL;
    }
}

void DP_liquify_decref(DP_Liquify *l)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);
    if (DP_atomic_dec(&l->refcount)) {
        map_decref_nullable(l, l->lm);
        DP_LiquifyTile *lt = l->free_tiles;
        while (lt) {
            DP_LiquifyTile *lt_to_free = lt;
            lt = lt->next_free;
            DP_free_simd(lt_to_free);
        }
        DP_mutex_free(l->mutex);
        DP_free(l);
    }
}

void DP_liquify_decref_nullable(DP_Liquify *l_or_null)
{
    if (l_or_null) {
        DP_liquify_decref(l_or_null);
    }
}

int DP_liquify_refcount(DP_Liquify *l)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);
    return DP_atomic_get(&l->refcount);
}

DP_LiquifyState *DP_liquify_current_state_inc(DP_Liquify *l)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);

    DP_LiquifyState *ls = DP_malloc(sizeof(*ls));
    ls->l = DP_liquify_incref(l);
    DP_atomic_set(&ls->refcount, 1);

    DP_Mutex *mutex = l->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    ls->lm = map_incref_nullable(l->lm);
    DP_MUTEX_MUST_UNLOCK(mutex);

    return ls;
}

bool DP_liquify_current_state_set_inc(DP_Liquify *l, DP_LiquifyState *ls)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);
    DP_ASSERT(ls);
    DP_ASSERT(DP_atomic_get(&ls->refcount) > 0);
    if (ls->l == l) {
        DP_liquify_state_apply(ls);
        return true;
    }
    else {
        return false;
    }
}

static float calculate_alpha(float distance, float radius, float hardness)
{
    if (distance < radius) {
        float core = radius * hardness;
        if (distance <= core) {
            return 1.0f;
        }
        else {
            return (radius - distance) / (radius - core);
        }
    }
    else {
        return 0.0f;
    }
}

static void op_transform(const DP_LiquifyOpParams *params, float xf, float yf,
                         float prev_x, float prev_y, float *out_x, float *out_y,
                         void (*fn)(const DP_LiquifyOpParams *params, float xf,
                                    float yf, float prev_x, float prev_y,
                                    float center_x, float center_y,
                                    float src_dx, float src_dy, float alpha,
                                    float *out_x, float *out_y))
{
    float src_x = xf - prev_x;
    float src_y = yf - prev_y;

    float center_x = params->x;
    float center_y = params->y;
    float src_dx = src_x - center_x;
    float src_dy = src_y - center_y;

    float src_distance =
        sqrtf(DP_square_float(src_dx) + DP_square_float(src_dy));
    float radius = params->radius;
    if (src_distance < radius) {
        float alpha = calculate_alpha(src_distance, radius, params->hardness);
        fn(params, xf, yf, prev_x, prev_y, center_x, center_y, src_dx, src_dy,
           alpha, out_x, out_y);
    }
    else {
        *out_x = prev_x;
        *out_y = prev_y;
    }
}

static void op_transform_move(const DP_LiquifyOpParams *params,
                              DP_UNUSED float xf, DP_UNUSED float yf,
                              float prev_x, float prev_y,
                              DP_UNUSED float center_x,
                              DP_UNUSED float center_y, DP_UNUSED float src_dx,
                              DP_UNUSED float src_dy, float alpha, float *out_x,
                              float *out_y)
{
    *out_x = prev_x + (params->move.dx * alpha);
    *out_y = prev_y + (params->move.dy * alpha);
}

static void op_transform_scale(const DP_LiquifyOpParams *params, float xf,
                               float yf, DP_UNUSED float prev_x,
                               DP_UNUSED float prev_y, float center_x,
                               float center_y, float src_dx, float src_dy,
                               float alpha, float *out_x, float *out_y)
{
    float ratio = DP_max_float(0.001f, 1.0f - params->scale.amount * alpha);
    *out_x = xf - center_x - (src_dx * ratio);
    *out_y = yf - center_y - (src_dy * ratio);
}

static void op_transform_rotate(const DP_LiquifyOpParams *params, float xf,
                                float yf, DP_UNUSED float prev_x,
                                DP_UNUSED float prev_y, float center_x,
                                float center_y, float src_dx, float src_dy,
                                float alpha, float *out_x, float *out_y)
{
    float theta = params->rotate.angle * alpha;
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    *out_x = xf - center_x - (src_dx * cos_theta) + (src_dy * sin_theta);
    *out_y = yf - center_y - (src_dx * sin_theta) - (src_dy * cos_theta);
}

static void op_smoothe(const DP_LiquifyOpParams *params,
                       DP_LiquifyMap *lm_or_null, float xf, float yf,
                       float distance_squared, float prev_x, float prev_y,
                       float *out_x, float *out_y)
{
    float kernel_radius = params->smoothe.kernel_radius;
    float lx, ly, rx, ry, tx, ty, bx, by;
    map_sample_pixel_bilinear(lm_or_null, xf - kernel_radius, yf, &lx, &ly);
    map_sample_pixel_bilinear(lm_or_null, xf + kernel_radius, yf, &rx, &ry);
    map_sample_pixel_bilinear(lm_or_null, xf, yf - kernel_radius, &tx, &ty);
    map_sample_pixel_bilinear(lm_or_null, xf, yf + kernel_radius, &bx, &by);

    float avg_x = (lx + rx + tx + bx) * 0.25f;
    float avg_y = (ly + ry + ty + by) * 0.25f;

    float alpha = calculate_alpha(sqrtf(distance_squared), params->radius,
                                  params->hardness);
    float ratio = DP_min_float(1.0f, alpha * params->smoothe.amount);

    *out_x = prev_x + ((avg_x - prev_x) * ratio);
    *out_y = prev_y + ((avg_y - prev_y) * ratio);
}

static void op_erase(const DP_LiquifyOpParams *params, float distance_squared,
                     float prev_x, float prev_y, float *out_x, float *out_y)
{
    float alpha = calculate_alpha(sqrtf(distance_squared), params->radius,
                                  params->hardness);
    float ratio = alpha * (1.0f - params->erase.amount);
    *out_x = (prev_x * (1.0f - alpha)) + prev_x * ratio;
    *out_y = (prev_y * (1.0f - alpha)) + prev_y * ratio;
}

bool DP_liquify_op(DP_Liquify *l, DP_DrawContext *dc,
                   const DP_LiquifyOpParams *params)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);
    DP_ASSERT(dc);
    DP_ASSERT(params);

    // Modifications may only be made from one thread, so no need to lock yet.
    float center_x = params->x;
    float center_y = params->y;
    float radius = params->radius;
    DP_Rect area = {
        DP_float_to_int(roundf(center_x - radius)),
        DP_float_to_int(roundf(center_y - radius)),
        DP_float_to_int(roundf(center_x + radius)),
        DP_float_to_int(roundf(center_y + radius)),
    };
    if (!DP_rect_valid(area)) {
        return false;
    }

    DP_LiquifyMap *old_lm = l->lm;
    DP_LiquifyOpType type = params->type;
    switch (type) {
    case DP_LIQUIFY_OP_TYPE_SMOOTHE:
    case DP_LIQUIFY_OP_TYPE_ERASE:
        // Don't need to bother if there's nothing to smoothe or erase.
        if (old_lm) {
            area = DP_rect_intersection(area, map_pixel_bounds(old_lm));
            if (DP_rect_valid(area)) {
                break;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    default:
        // For other operations, there's no need to bother if the user is
        // drawing outside of any bounds. This isn't super accurate, the user
        // may still just be dragging transparency around and they can cause the
        // liquify image to expand if they keep dragging that nothing further.
        // Still, this should cover realistic cases where the user accidentally
        // brushes around way off base or something.
        if (DP_rect_intersects(
                area,
                old_lm ? DP_rect_union(map_pixel_bounds(old_lm), l->mask_rect)
                       : l->mask_rect)) {
            break;
        }
        else {
            return false;
        }
    }

    int width = DP_rect_width(area);
    int height = DP_rect_height(area);
    size_t area_size = DP_int_to_size(width) * DP_int_to_size(height);
    float *xs =
        DP_draw_context_pool_require(dc, area_size * sizeof(*xs) * (size_t)2);
    float *ys = xs + area_size;

    float radius_squared = DP_square_float(radius);
    int out_index = 0;
    for (int y = area.y1; y <= area.y2; ++y) {
        float yf = DP_int_to_float(y);
        float dy = yf - center_y;
        float dy_squared = DP_square_float(dy);

        for (int x = area.x1; x <= area.x2; ++x) {
            float xf = DP_int_to_float(x);
            float dx = xf - center_x;
            float dx_squared = DP_square_float(dx);
            float distance_squared = dx_squared + dy_squared;

            float prev_x, prev_y;
            map_get_pixel_nullable(old_lm, x, y, &prev_x, &prev_y);

            float out_x, out_y;
            if (distance_squared < radius_squared) {
                switch (type) {
                case DP_LIQUIFY_OP_TYPE_MOVE:
                    op_transform(params, xf, yf, prev_x, prev_y, &out_x, &out_y,
                                 op_transform_move);
                    break;
                case DP_LIQUIFY_OP_TYPE_SCALE:
                    op_transform(params, xf, yf, prev_x, prev_y, &out_x, &out_y,
                                 op_transform_scale);
                    break;
                case DP_LIQUIFY_OP_TYPE_ROTATE:
                    op_transform(params, xf, yf, prev_x, prev_y, &out_x, &out_y,
                                 op_transform_rotate);
                    break;
                case DP_LIQUIFY_OP_TYPE_SMOOTHE:
                    op_smoothe(params, old_lm, xf, yf, distance_squared, prev_x,
                               prev_y, &out_x, &out_y);
                    break;
                case DP_LIQUIFY_OP_TYPE_ERASE:
                    op_erase(params, distance_squared, prev_x, prev_y, &out_x,
                             &out_y);
                    break;
                default:
                    DP_UNREACHABLE();
                }
            }
            else {
                out_x = prev_x;
                out_y = prev_y;
            }

            xs[out_index] = fabsf(out_x) > 1e-3 ? out_x : 0.0f;
            ys[out_index] = fabsf(out_y) > 1e-3 ? out_y : 0.0f;
            ++out_index;
        }
    }

    // This gets updated below to include the existing tile bounds.
    DP_Rect tile_bounds = DP_tile_area_make(area);

    DP_Mutex *mutex = l->mutex;
    DP_MUTEX_MUST_LOCK(mutex);

    DP_LiquifyMap *lm;
    if (old_lm) {
        tile_bounds = DP_rect_union(tile_bounds, old_lm->tile_bounds);
        lm = map_make_editable(old_lm, tile_bounds);
    }
    else {
        lm = map_alloc_zeroed(tile_bounds);
    }

    bool any_blanked = false;
    DP_TileIterator ti = DP_tile_iterator_make_with(area, NULL);
    while (DP_tile_iterator_next(&ti)) {
        DP_LiquifyTile *lt =
            tile_make_editable_nullable(l, map_get_tile(lm, ti.col, ti.row));

        bool maybe_blank = false;
        bool has_fill = false;

        DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
        while (DP_tile_into_dst_iterator_next(&tidi)) {
            int index = tidi.dst_y * width + tidi.dst_x;
            float dx = xs[index];
            float dy = ys[index];
            tile_set_xy(lt, tidi.tile_x, tidi.tile_y, dx, dy);
            if (dx == 0.0f && dy == 0.0f) {
                maybe_blank = true;
            }
            else {
                has_fill = true;
            }
        }

        if (maybe_blank && !has_fill && tile_is_blank(lt)) {
            any_blanked = true;
            map_clear_tile(l, lm, ti.col, ti.row, lt);
        }
        else {
            map_set_tile_noinc(l, lm, ti.col, ti.row, lt);
        }
    }

    // Crop blank tiles from the edges. If there's no tiles left, clear the map.
    if (any_blanked) {
        int min_x = INT_MAX;
        int max_x = INT_MIN;
        int min_y = INT_MAX;
        int max_y = INT_MIN;

        for (int ty = tile_bounds.y1; ty <= tile_bounds.y2; ++ty) {
            for (int tx = tile_bounds.x1; tx <= tile_bounds.x2; ++tx) {
                if (map_get_tile(lm, tx, ty)) {
                    if (tx < min_x) {
                        min_x = tx;
                    }
                    if (tx > max_x) {
                        max_x = tx;
                    }
                    if (ty < min_y) {
                        min_y = ty;
                    }
                    if (ty > max_y) {
                        max_y = ty;
                    }
                }
            }
        }

        if (min_x > max_x || min_y > max_y) {
            if (lm != old_lm) {
                map_decref(l, lm);
            }
            lm = NULL;
        }
        else if (min_x != tile_bounds.x1 || min_y != tile_bounds.y1
                 || max_x != tile_bounds.x2 || max_y != tile_bounds.y2) {
            int new_width = max_x - min_x + 1;
            int new_height = max_y - min_y + 1;
            int old_width = DP_rect_width(tile_bounds);
            int shift_x = min_x - tile_bounds.x1;
            int shift_y = min_y - tile_bounds.y1;
            for (int ty = 0; ty < new_height; ++ty) {
                DP_LiquifyTile **dst_row = &lm->tiles[ty * new_width];
                DP_LiquifyTile **src_row =
                    &lm->tiles[(shift_y + ty) * old_width + shift_x];
                memmove(dst_row, src_row,
                        DP_int_to_size(new_width) * sizeof(*dst_row));
            }
            lm->tile_bounds = (DP_Rect){min_x, min_y, max_x, max_y};
        }
    }

    if (lm != old_lm) {
        l->lm = lm;
        map_decref_nullable(l, old_lm);
    }

    DP_MUTEX_MUST_UNLOCK(mutex);
    return true;
}


DP_LiquifyState *DP_liquify_state_incref(DP_LiquifyState *ls)
{
    DP_ASSERT(ls);
    DP_ASSERT(DP_atomic_get(&ls->refcount) > 0);
    DP_atomic_inc(&ls->refcount);
    return ls;
}

DP_LiquifyState *DP_liquify_state_incref_nullable(DP_LiquifyState *ls_or_null)
{
    if (ls_or_null) {
        return DP_liquify_state_incref(ls_or_null);
    }
    else {
        return NULL;
    }
}

void DP_liquify_state_decref(DP_LiquifyState *ls)
{
    DP_ASSERT(ls);
    DP_ASSERT(DP_atomic_get(&ls->refcount) > 0);
    if (DP_atomic_dec(&ls->refcount)) {
        map_decref_nullable(ls->l, ls->lm);
        DP_liquify_decref(ls->l);
        DP_free(ls);
    }
}

void DP_liquify_state_decref_nullable(DP_LiquifyState *ls_or_null)
{
    if (ls_or_null) {
        DP_liquify_state_decref(ls_or_null);
    }
}

int DP_liquify_state_refcount(DP_LiquifyState *ls)
{
    DP_ASSERT(ls);
    DP_ASSERT(DP_atomic_get(&ls->refcount) > 0);
    return DP_atomic_get(&ls->refcount);
}

void DP_liquify_state_apply(DP_LiquifyState *ls)
{
    DP_ASSERT(ls);
    DP_ASSERT(DP_atomic_get(&ls->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&ls->l->refcount) > 0);

    DP_Liquify *l = ls->l;
    DP_LiquifyMap *new_lm = ls->lm;

    DP_Mutex *mutex = l->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    DP_LiquifyMap *old_lm = l->lm;
    if (new_lm != old_lm) {
        map_decref_nullable(l, old_lm);
        l->lm = map_incref_nullable(new_lm);
    }
    DP_MUTEX_MUST_UNLOCK(mutex);
}


static DP_Rect liquify_image_bounds(DP_LiquifyImage *li)
{
    return DP_rect_make(li->x, li->y, li->width, li->height);
}

static uint32_t liquify_image_get(const DP_LiquifyImage *li, int x, int y)
{
    DP_ASSERT(li);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x < li->width);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y < li->height);
    return li->data[y * li->width + x];
}

static uint32_t liquify_image_get_checked(const DP_LiquifyImage *li, int x,
                                          int y)
{
    DP_ASSERT(li);
    if (x >= 0 && y >= 0 && x < li->width && y < li->height) {
        return liquify_image_get(li, x, y);
    }
    else {
        return 0;
    }
}

static void liquify_image_set(const DP_LiquifyImage *li, int x, int y,
                              uint32_t value)
{
    DP_ASSERT(li);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x < li->width);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y < li->height);
    li->data[y * li->width + x] = value;
}


DP_LiquifyTransformer *DP_liquify_transformer_new(int source_x, int source_y,
                                                  int source_width,
                                                  int source_height,
                                                  const uint32_t *source_data)
{
    DP_ASSERT(source_width > 0);
    DP_ASSERT(source_height > 0);
    DP_ASSERT(source_data);
    DP_LiquifyTransformer *ltr = DP_malloc(sizeof(*ltr));
    size_t source_size = DP_int_to_size(source_width)
                       * DP_int_to_size(source_height) * sizeof(*source_data);
    *ltr = (DP_LiquifyTransformer){
        NULL,
        {DP_memdup(source_data, source_size), source_x, source_y, source_width,
         source_height},
        {NULL, 0, 0, 0, 0},
#if DP_LIQUIFY_DEBUG_OVERLAY
        {NULL, 0, 0, 0, 0},
#endif
        -1,
        DP_ATOMIC_INIT(1),
    };
    return ltr;
}

DP_LiquifyTransformer *DP_liquify_transformer_incref(DP_LiquifyTransformer *ltr)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);
    DP_atomic_inc(&ltr->refcount);
    return ltr;
}

DP_LiquifyTransformer *
DP_liquify_transformer_incref_nullable(DP_LiquifyTransformer *ltr_or_null)
{
    if (ltr_or_null) {
        return DP_liquify_transformer_incref(ltr_or_null);
    }
    else {
        return NULL;
    }
}

void DP_liquify_transformer_decref(DP_LiquifyTransformer *ltr)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);
    if (DP_atomic_dec(&ltr->refcount)) {
#if DP_LIQUIFY_DEBUG_OVERLAY
        DP_free(ltr->debug.data);
#endif
        DP_free(ltr->target.data);
        DP_free(ltr->source.data);
        DP_liquify_state_decref_nullable(ltr->ls);
        DP_free(ltr);
    }
}

void DP_liquify_transformer_decref_nullable(DP_LiquifyTransformer *ltr_or_null)
{
    if (ltr_or_null) {
        DP_liquify_transformer_decref(ltr_or_null);
    }
}

int DP_liquify_transformer_refcount(DP_LiquifyTransformer *ltr)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);
    return DP_atomic_get(&ltr->refcount);
}

int DP_liquify_transformer_source_x(DP_LiquifyTransformer *ltr)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);
    return ltr->source.x;
}

int DP_liquify_transformer_source_y(DP_LiquifyTransformer *ltr)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);
    return ltr->source.y;
}

static DP_LiquifyImage
liquify_transformer_init_target(DP_Rect source_bounds, DP_Rect target_bounds,
                                const uint32_t *source_data)
{

    int target_x = DP_rect_x(target_bounds);
    int target_y = DP_rect_y(target_bounds);
    int target_width = DP_rect_width(target_bounds);
    int target_height = DP_rect_height(target_bounds);

    int source_width = DP_rect_width(source_bounds);
    int source_height = DP_rect_height(source_bounds);
    size_t source_line_size =
        DP_int_to_size(source_width) * sizeof(*source_data);

    uint32_t *target_data;
    if (DP_rect_equal(target_bounds, source_bounds)) {
        size_t source_size = DP_int_to_size(source_height) * source_line_size;
        target_data = DP_memdup(source_data, source_size);
    }
    else {
        target_data = DP_malloc_zeroed(DP_int_to_size(target_width)
                                       * DP_int_to_size(target_height)
                                       * sizeof(*target_data));
        DP_Rect copy_rect = DP_rect_intersection(source_bounds, target_bounds);
        if (DP_rect_valid(copy_rect)) {
            int copy_width = DP_rect_width(copy_rect);
            int copy_height = DP_rect_height(copy_rect);
            size_t copy_line_size =
                DP_int_to_size(copy_width) * sizeof(*target_data);

            int source_shift_x = copy_rect.x1 - source_bounds.x1;
            int source_shift_y = copy_rect.y1 - source_bounds.y1;
            int target_shift_x = copy_rect.x1 - target_bounds.x1;
            int target_shift_y = copy_rect.y1 - target_bounds.y1;

            for (int y = 0; y < copy_height; ++y) {
                memcpy(&target_data[((target_shift_y + y) * target_width)
                                    + target_shift_x],
                       &source_data[((source_shift_y + y) * source_width)
                                    + source_shift_x],
                       copy_line_size);
            }
        }
    }

    return (DP_LiquifyImage){
        target_data, target_x, target_y, target_width, target_height,
    };
}

static void liquify_transformer_reset_tile(DP_TileIterator *ti,
                                           DP_LiquifyImage *source,
                                           DP_LiquifyImage *target, int tsx,
                                           int tsy)
{
    DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(ti);
    while (DP_tile_into_dst_iterator_next(&tidi)) {
        int tx = tidi.dst_x;
        int ty = tidi.dst_y;
        int sx = tx + tsx;
        int sy = ty + tsy;
        liquify_image_set(target, tidi.dst_x, tidi.dst_y,
                          liquify_image_get_checked(source, sx, sy));
    }
}

static void
liquify_transformer_apply_tile(DP_TileIterator *ti, DP_LiquifyTile *lt,
                               DP_LiquifyImage *source, DP_LiquifyImage *target,
                               int interpolation, float eps, int tsx, int tsy)
{
    DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(ti);
    while (DP_tile_into_dst_iterator_next(&tidi)) {
        int tx = tidi.dst_x;
        int ty = tidi.dst_y;
        int sx = tx + tsx;
        int sy = ty + tsy;
        float dx, dy;
        tile_get_xy(lt, tidi.tile_x, tidi.tile_y, &dx, &dy);
        if (fabsf(dx) < eps && fabsf(dy) < eps) {
            liquify_image_set(target, tx, ty,
                              liquify_image_get_checked(source, sx, sy));
        }
        else {
            liquify_image_set(
                target, tx, ty,
                DP_image_transform_fetch_blank(
                    interpolation, source->width, source->height, source->data,
                    DP_int_to_double(sx) - DP_float_to_double(dx),
                    DP_int_to_double(sy) - DP_float_to_double(dy)));
        }
    }
}

static void liquify_transformer_reset_tiles(DP_LiquifyMap *old_lm,
                                            DP_LiquifyImage *source,
                                            DP_LiquifyImage *target,
                                            DP_Rect image_bounds, int tsx,
                                            int tsy)
{
    DP_ASSERT(old_lm);
    DP_ASSERT(source->x == target->x);
    DP_ASSERT(source->y == target->y);
    DP_ASSERT(source->width == target->width);
    DP_ASSERT(source->height == target->height);
    DP_ASSERT(source->data);
    DP_ASSERT(target->data);
    DP_Rect old_lm_bounds = map_pixel_bounds(old_lm);
    DP_TileIterator ti =
        DP_tile_iterator_make_with(image_bounds, &old_lm_bounds);
    while (DP_tile_iterator_next(&ti)) {
        DP_LiquifyTile *lt = map_get_tile(old_lm, ti.col, ti.row);
        if (lt) {
            liquify_transformer_reset_tile(&ti, source, target, tsx, tsy);
        }
    }
}

static void
liquify_transformer_apply_diff(DP_LiquifyMap *lm, DP_LiquifyMap *old_lm,
                               DP_LiquifyImage *source, DP_LiquifyImage *target,
                               DP_Rect target_bounds, DP_Rect old_target_bounds,
                               DP_Rect lm_bounds, int interpolation,
                               int old_interpolation, int tsx, int tsy)
{
    if (!DP_rect_equal(target_bounds, old_target_bounds)) {
        uint32_t *old_target_data = target->data;
        *target = liquify_transformer_init_target(
            old_target_bounds, target_bounds, old_target_data);
        DP_free(old_target_data);
    }

    float eps = DP_image_transform_epsilon(interpolation);
    bool interpolation_changed = interpolation != old_interpolation;
    DP_TileIterator ti = DP_tile_iterator_make_with(target_bounds, &lm_bounds);
    while (DP_tile_iterator_next(&ti)) {
        DP_LiquifyTile *lt = map_get_tile(lm, ti.col, ti.row);
        DP_LiquifyTile *old_lt = map_get_tile_checked(old_lm, ti.col, ti.row);
        if (lt) {
            if (lt != old_lt || interpolation_changed) {
                liquify_transformer_apply_tile(&ti, lt, source, target,
                                               interpolation, eps, tsx, tsy);
            }
        }
        else if (old_lt) {
            liquify_transformer_reset_tile(&ti, source, target, tsx, tsy);
        }
    }
}

static void
liquify_transformer_apply_tiles_new(DP_LiquifyMap *lm, DP_LiquifyImage *source,
                                    DP_LiquifyImage *target,
                                    DP_Rect target_bounds, DP_Rect lm_bounds,
                                    int interpolation, int tsx, int tsy)
{
    float eps = DP_image_transform_epsilon(interpolation);
    DP_TileIterator ti = DP_tile_iterator_make_with(target_bounds, &lm_bounds);
    while (DP_tile_iterator_next(&ti)) {
        DP_LiquifyTile *lt = map_get_tile(lm, ti.col, ti.row);
        if (lt) {
            liquify_transformer_apply_tile(&ti, lt, source, target,
                                           interpolation, eps, tsx, tsy);
        }
    }
}

static void liquify_transformer_reset(DP_LiquifyTransformer *ltr,
                                      DP_LiquifyMap *old_lm,
                                      DP_Rect source_bounds)
{
    if (old_lm) {
        DP_ASSERT(ltr->target.data);
        DP_Rect old_target_bounds = liquify_image_bounds(&ltr->target);
        if (DP_rect_equal(source_bounds, old_target_bounds)) {
            // Resetting existing image, same bounds. Replace all changed tiles
            // with the pixels from the source.
            liquify_transformer_reset_tiles(old_lm, &ltr->source, &ltr->target,
                                            source_bounds, 0, 0);
        }
        else {
            // Resetting existing image, different bounds. Just clone the
            // source, no point shuffling stuff around.
            DP_free(ltr->target.data);
            ltr->target = liquify_transformer_init_target(
                source_bounds, source_bounds, ltr->source.data);
        }
    }
    else {
        // Reset new image, just clone the source.
        DP_ASSERT(!ltr->target.data);
        ltr->target = liquify_transformer_init_target(
            source_bounds, source_bounds, ltr->source.data);
    }
}

static bool liquify_transformer_apply_state(DP_LiquifyTransformer *ltr,
                                            DP_LiquifyState *ls,
                                            DP_LiquifyMap *old_lm,
                                            int interpolation)
{
    DP_Rect source_bounds = liquify_image_bounds(&ltr->source);

    DP_LiquifyMap *lm = ls->lm;
    if (lm) {
        DP_Rect lm_bounds = map_pixel_bounds(lm);
        DP_Rect target_bounds = DP_rect_union(lm_bounds, source_bounds);
        int tsx = DP_rect_x(target_bounds) - DP_rect_x(source_bounds);
        int tsy = DP_rect_y(target_bounds) - DP_rect_y(source_bounds);
        if (old_lm) {
            DP_ASSERT(ltr->target.data);
            DP_Rect old_target_bounds = liquify_image_bounds(&ltr->target);
            // Modifying existing image. Copy the target pixels if bounds
            // changed, do a diff, replace changed and add new tiles.
            liquify_transformer_apply_diff(
                lm, old_lm, &ltr->source, &ltr->target, target_bounds,
                old_target_bounds, lm_bounds, interpolation, ltr->interpolation,
                tsx, tsy);
        }
        else {
            // New image, initialize from the source, apply the map. If there is
            // an existing target, it is just a copy of the source here.
            if (ltr->target.data) {
                DP_Rect old_target_bounds = liquify_image_bounds(&ltr->target);
                if (!DP_rect_equal(target_bounds, old_target_bounds)) {
                    DP_free(ltr->target.data);
                    ltr->target = liquify_transformer_init_target(
                        source_bounds, target_bounds, ltr->source.data);
                }
            }
            else {
                ltr->target = liquify_transformer_init_target(
                    source_bounds, target_bounds, ltr->source.data);
            }
            liquify_transformer_apply_tiles_new(lm, &ltr->source, &ltr->target,
                                                target_bounds, lm_bounds,
                                                interpolation, tsx, tsy);
        }
    }
    else {
        liquify_transformer_reset(ltr, old_lm, source_bounds);
    }

    return true;
}

static bool liquify_transformer_apply(DP_LiquifyTransformer *ltr,
                                      DP_LiquifyState *ls_or_null,
                                      int interpolation)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);

    DP_LiquifyState *old_ls = ltr->ls;
    if (ls_or_null) {
        if (old_ls) {
            if (ls_or_null->lm == old_ls->lm) {
                if (interpolation == ltr->interpolation) {
                    // Just the same map, nothing changed.
                    DP_liquify_state_decref(old_ls);
                    ltr->ls = DP_liquify_state_incref(ls_or_null);
                    return false;
                }
                else {
                    // Same map, but interpolation changed, recalculate tiles.
                    return liquify_transformer_apply_state(
                        ltr, ls_or_null, old_ls->lm, interpolation);
                }
            }
            else {
                // Different map, do a full diff.
                bool changed = liquify_transformer_apply_state(
                    ltr, ls_or_null, old_ls->lm, interpolation);
                DP_liquify_state_decref(old_ls);
                ltr->ls = DP_liquify_state_incref(ls_or_null);
                return changed;
            }
        }
        else {
            // New map, initialize it.
            bool changed = liquify_transformer_apply_state(ltr, ls_or_null,
                                                           NULL, interpolation);
            ltr->ls = DP_liquify_state_incref(ls_or_null);
            return changed;
        }
    }
    else if (old_ls) {
        // Going from having a state to not having one. Reset the target.
        liquify_transformer_reset(ltr, old_ls->lm,
                                  liquify_image_bounds(&ltr->source));
        DP_liquify_state_decref(old_ls);
        ltr->ls = NULL;
        return true;
    }
    else {
        // Didn't have a state, still don't have one.
        return false;
    }
}

#if DP_LIQUIFY_DEBUG_OVERLAY
static DP_Pixel8 liquify_transformer_apply_debug_overlay_color(float dx,
                                                               float dy)
{
    if (dx == 0.0f && dy == 0.0f) {
        return (DP_Pixel8){0xffffffffu};
    }
    else {
        // Scale the angle to [0, 1] to get a sensible hue.
        float angle = atan2f(dy, dx);
        float r = (angle + (float)M_PI) / (2.0f * (float)M_PI);

        // Saturation shows small length variations, value large ones.
        float length = sqrtf(DP_square_float(dx) + DP_square_float(dy));
        float g = length;
        float b = 1.0f - (length / 100.0f);

        hsv_to_rgb_float(&r, &g, &b);
        return (DP_Pixel8){.b = DP_channel_float_to_8(b),
                           .g = DP_channel_float_to_8(g),
                           .r = DP_channel_float_to_8(r),
                           .a = UINT8_MAX};
    }
}

static void liquify_transformer_apply_debug_overlay(DP_LiquifyTransformer *ltr)
{

    DP_LiquifyState *ls = ltr->ls;
    DP_LiquifyImage *debug = &ltr->debug;
    if (!ls) {
        DP_free(debug->data);
        debug->data = NULL;
        return;
    }

    DP_LiquifyMap *lm = ls->lm;
    if (!lm) {
        DP_free(debug->data);
        debug->data = NULL;
        return;
    }

    DP_LiquifyImage *target = &ltr->target;
    DP_free(debug->data);
    *debug = *target;
    if (!target->data) {
        return;
    }

    DP_Rect target_bounds = liquify_image_bounds(target);
    debug->data = DP_memdup(target->data,
                            DP_int_to_size(DP_rect_width(target_bounds))
                                * DP_int_to_size(DP_rect_height(target_bounds))
                                * sizeof(*target->data));

    DP_Rect lm_bounds = map_pixel_bounds(lm);
    DP_TileIterator ti = DP_tile_iterator_make_with(target_bounds, &lm_bounds);
    while (DP_tile_iterator_next(&ti)) {
        DP_LiquifyTile *lt = map_get_tile(lm, ti.col, ti.row);
        DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
        while (DP_tile_into_dst_iterator_next(&tidi)) {
            DP_Pixel8 overlay_pixel;
            if (lt) {
                float dx, dy;
                tile_get_xy(lt, tidi.tile_x, tidi.tile_y, &dx, &dy);
                overlay_pixel =
                    liquify_transformer_apply_debug_overlay_color(dx, dy);
            }
            else {
                overlay_pixel = (DP_Pixel8){0xff000000u};
            }

            DP_Pixel8 base_pixel =
                (DP_Pixel8){liquify_image_get(target, tidi.dst_x, tidi.dst_y)};
            DP_Pixel8 result_pixel = DP_blend_pixel8(base_pixel, overlay_pixel,
                                                     (uint8_t)(UINT8_MAX / 2));
            liquify_image_set(debug, tidi.dst_x, tidi.dst_y,
                              result_pixel.color);
        }
    }
}
#endif

bool DP_liquify_transformer_apply(DP_LiquifyTransformer *ltr,
                                  DP_LiquifyState *ls_or_null,
                                  int interpolation)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);

    bool changed = liquify_transformer_apply(ltr, ls_or_null, interpolation);
    ltr->interpolation = interpolation;

#if DP_LIQUIFY_DEBUG_OVERLAY
    liquify_transformer_apply_debug_overlay(ltr);
#endif

    return changed;
}

bool DP_liquify_transformer_target_image(DP_LiquifyTransformer *ltr, int *out_x,
                                         int *out_y, int *out_width,
                                         int *out_height,
                                         const uint32_t **out_data)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);

#if DP_LIQUIFY_DEBUG_OVERLAY
    DP_LiquifyImage *target = &ltr->debug;
#else
    DP_LiquifyImage *target = &ltr->target;
#endif
    if (target->data) {
        if (out_x) {
            *out_x = target->x;
        }
        if (out_y) {
            *out_y = target->y;
        }
        if (out_width) {
            *out_width = target->width;
        }
        if (out_height) {
            *out_height = target->height;
        }
        if (out_data) {
            *out_data = target->data;
        }
        return true;
    }
    else {
        return false;
    }
}
