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
    int mask_x;
    int mask_y;
    int mask_width;
    int mask_height;
    unsigned char mask[];
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
    DP_Atomic refcount;
};


typedef void (*DP_LiquifyOpFn)(const DP_LiquifyOpParams *params,
                               DP_LiquifyMap *lm_or_null, int x, int y,
                               float alpha, float *out_x, float *out_y);


// TODO make mask optional, if it would be all 255
static unsigned char mask_at(DP_Liquify *l, int x, int y)
{
    int mx = x - l->mask_x;
    int my = y - l->mask_y;
    int width = l->mask_width;
    if (mx >= 0 && my >= 0 && mx < width && my < l->mask_height) {
        return l->mask[my * width + mx];
    }
    else {
        return 0;
    }
}


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
        for (int i = 0; i < (int)DP_ARRAY_LENGTH(lt->d); ++i) {
            lt->d[i] = 0.0f;
        }
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


static size_t map_tile_count(DP_Rect tile_bounds)
{
    return DP_int_to_size(DP_rect_width(tile_bounds))
         * DP_int_to_size(DP_rect_height(tile_bounds));
}

static DP_LiquifyMap *map_alloc(DP_Rect tile_bounds)
{
    DP_ASSERT(!DP_rect_empty(tile_bounds));
    size_t count = map_tile_count(tile_bounds);
    DP_LiquifyMap *map = DP_malloc(DP_FLEX_SIZEOF(DP_LiquifyMap, tiles, count));
    map->refcount = 1;
    map->tile_bounds = tile_bounds;
    return map;
}

static DP_LiquifyMap *map_alloc_zeroed(DP_Rect tile_bounds)
{
    DP_ASSERT(!DP_rect_empty(tile_bounds));
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
    int tx = x / DP_TILE_SIZE;
    int ty = y / DP_TILE_SIZE;
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

static void map_sample_pixel_bilinear_nullable(DP_LiquifyMap *lm_or_null,
                                               float xf, float yf,
                                               float *out_dx, float *out_dy)
{
    if (lm_or_null) {
        map_sample_pixel_bilinear(lm_or_null, xf, yf, out_dx, out_dy);
    }
    else {
        *out_dx = 0.0f;
        *out_dy = 0.0f;
    }
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
                           int mask_height,
                           void (*fill_mask)(void *, unsigned char *),
                           void *user)
{
    DP_ASSERT(fill_mask);
    DP_ASSERT(mask_width > 0);
    DP_ASSERT(mask_height > 0);

    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return NULL;
    }

    size_t mask_size = DP_int_to_size(mask_width) * DP_int_to_size(mask_height);
    DP_Liquify *l = DP_malloc(DP_FLEX_SIZEOF(DP_Liquify, mask, mask_size));
    l->mutex = mutex;
    l->lm = NULL;
    l->free_tiles = NULL;
    DP_atomic_set(&l->refcount, 1);
    l->mask_x = mask_x;
    l->mask_y = mask_y;
    l->mask_width = mask_width;
    l->mask_height = mask_height;
    fill_mask(user, l->mask);
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

static uint32_t liquify_dump_color(float dx, float dy)
{
    if (dx == 0.0f && dy == 0.0f) {
        return (uint32_t)0xffffffffu;
    }
    else {
        return (uint32_t)0xff000000u;
    }
}

uint32_t *DP_liquify_dump(DP_Liquify *l, int *out_width, int *out_height)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);

    DP_Mutex *mutex = l->mutex;
    DP_MUTEX_MUST_LOCK(mutex);

    int width, height;
    uint32_t *data;
    DP_LiquifyMap *lm = l->lm;
    if (lm) {
        DP_Rect bounds = map_pixel_bounds(lm);
        width = DP_rect_width(bounds);
        height = DP_rect_height(bounds);
        data = DP_malloc_zeroed(DP_int_to_size(width) * DP_int_to_size(height)
                                * sizeof(*data));
        DP_TileIterator ti =
            DP_tile_iterator_make(UINT16_MAX, UINT16_MAX, bounds);
        while (DP_tile_iterator_next(&ti)) {
            DP_LiquifyTile *lt = map_get_tile(lm, ti.col, ti.row);
            if (lt) {
                DP_TileIntoDstIterator tidi =
                    DP_tile_into_dst_iterator_make(&ti);
                while (DP_tile_into_dst_iterator_next(&tidi)) {
                    float dx, dy;
                    tile_get_xy(lt, tidi.tile_x, tidi.tile_y, &dx, &dy);
                    data[tidi.dst_y * width + tidi.dst_x] =
                        liquify_dump_color(dx, dy);
                }
            }
        }
    }
    else {
        data = NULL;
        width = 0;
        height = 0;
    }

    DP_MUTEX_MUST_UNLOCK(mutex);

    if (out_width) {
        *out_width = width;
    }
    if (out_height) {
        *out_height = height;
    }
    return data;
}

static float calculate_alpha(float distance, float radius)
{
    return DP_square_float((1.0f - DP_square_float(distance / radius)));
}

static void op_move(const DP_LiquifyOpParams *params, DP_LiquifyMap *lm_or_null,
                    int x, int y, DP_UNUSED float distance_squared,
                    float *out_x, float *out_y)
{
    float xf = DP_int_to_float(x);
    float yf = DP_int_to_float(y);

    float prev_x, prev_y;
    map_sample_pixel_bilinear_nullable(lm_or_null, xf, yf, &prev_x, &prev_y);

    float src_x = xf - prev_x;
    float src_y = yf - prev_y;

    float center_x = params->x;
    float center_y = params->y;
    float src_dx = src_x - center_x;
    float src_dy = src_y - center_y;

    float src_distance =
        sqrtf(DP_square_float(src_dx) + DP_square_float(src_dy));
    float radius = params->radius;
    float strength;
    if (src_distance < radius) {
        float fade = (1.0f - DP_square_float(src_distance / radius));
        strength = DP_square_float(fade);
    }
    else {
        strength = 0.0f;
    }

    float a_x = xf - (params->move.dx * strength);
    float a_y = yf - (params->move.dy * strength);

    *out_x = (xf - a_x) + prev_x;
    *out_y = (yf - a_y) + prev_y;
}

static void op_scale(const DP_LiquifyOpParams *params,
                     DP_LiquifyMap *lm_or_null, int x, int y,
                     DP_UNUSED float distance_squared, float *out_x,
                     float *out_y)
{
    float xf = DP_int_to_float(x);
    float yf = DP_int_to_float(y);

    float prev_x, prev_y;
    map_sample_pixel_bilinear_nullable(lm_or_null, xf, yf, &prev_x, &prev_y);

    float src_x = xf - prev_x;
    float src_y = yf - prev_y;

    float center_x = params->x;
    float center_y = params->y;
    float src_dx = src_x - center_x;
    float src_dy = src_y - center_y;

    // Don't grab stuff from outside the brush area. Smoothly fade out the edges
    // to avoid jitter as the brush is moved.
    float src_distance =
        sqrtf(DP_square_float(src_dx) + DP_square_float(src_dy));
    float radius = params->radius;
    float effective_amount;
    if (src_distance < radius) {
        float fade = (1.0f - DP_square_float(src_distance / radius));
        effective_amount = params->scale.amount * DP_square_float(fade);
    }
    else {
        effective_amount = 0.0f;
    }

    // Clamp the ratio so that bloating doesn't end up inverting.
    float ratio = DP_max_float(0.001f, 1.0f - effective_amount);
    *out_x = xf - center_x - (src_dx * ratio);
    *out_y = yf - center_y - (src_dy * ratio);
}

static void op_rotate(const DP_LiquifyOpParams *params,
                      DP_LiquifyMap *lm_or_null, int x, int y,
                      DP_UNUSED float distance_squared, float *out_x,
                      float *out_y)
{
    float xf = DP_int_to_float(x);
    float yf = DP_int_to_float(y);

    float prev_x, prev_y;
    map_sample_pixel_bilinear_nullable(lm_or_null, xf, yf, &prev_x, &prev_y);

    float src_x = xf - prev_x;
    float src_y = yf - prev_y;

    float center_x = params->x;
    float center_y = params->y;
    float src_dx = src_x - center_x;
    float src_dy = src_y - center_y;

    float src_distance =
        sqrtf(DP_square_float(src_dx) + DP_square_float(src_dy));
    float radius = params->radius;
    float theta;
    if (src_distance < radius) {
        float fade = (1.0f - DP_square_float(src_distance / radius));
        theta = params->rotate.angle * DP_square_float(fade);
    }
    else {
        theta = 0.0f;
    }

    float cos_t = cosf(theta);
    float sin_t = sinf(theta);
    float rot_rx = (src_dx * cos_t) - (src_dy * sin_t);
    float rot_ry = (src_dx * sin_t) + (src_dy * cos_t);

    *out_x = xf - center_x - rot_rx;
    *out_y = yf - center_y - rot_ry;
}

static void op_smoothe(const DP_LiquifyOpParams *params,
                       DP_LiquifyMap *lm_or_null, int x, int y,
                       float distance_squared, float *out_x, float *out_y)
{
    if (lm_or_null) {
        float xf = DP_int_to_float(x);
        float yf = DP_int_to_float(y);

        // FIXME: These don't need to sample bilinear? Same above
        float cur_x, cur_y;
        map_sample_pixel_bilinear(lm_or_null, xf, yf, &cur_x, &cur_y);

        float kernel_radius = params->smoothe.kernel_radius;
        float lx, ly, rx, ry, tx, ty, bx, by;
        map_sample_pixel_bilinear(lm_or_null, xf - kernel_radius, yf, &lx, &ly);
        map_sample_pixel_bilinear(lm_or_null, xf + kernel_radius, yf, &rx, &ry);
        map_sample_pixel_bilinear(lm_or_null, xf, yf - kernel_radius, &tx, &ty);
        map_sample_pixel_bilinear(lm_or_null, xf, yf + kernel_radius, &bx, &by);

        float avg_x = (lx + rx + tx + bx) * 0.25f;
        float avg_y = (ly + ry + ty + by) * 0.25f;

        float alpha = calculate_alpha(sqrtf(distance_squared), params->radius);
        float ratio = DP_min_float(1.0f, alpha * params->smoothe.amount);

        *out_x = cur_x + ((avg_x - cur_x) * ratio);
        *out_y = cur_y + ((avg_y - cur_y) * ratio);
    }
    else {
        *out_x = 0.0f;
        *out_y = 0.0f;
    }
}

static void op_erase(const DP_LiquifyOpParams *params,
                     DP_LiquifyMap *lm_or_null, int x, int y,
                     float distance_squared, float *out_x, float *out_y)
{
    if (lm_or_null) {
        float prev_x, prev_y;
        map_get_pixel(lm_or_null, x, y, &prev_x, &prev_y);

        float alpha = calculate_alpha(sqrtf(distance_squared), params->radius);
        float ratio = alpha * (1.0f - params->erase.amount);
        *out_x = (prev_x * (1.0f - alpha)) + prev_x * ratio;
        *out_y = (prev_y * (1.0f - alpha)) + prev_y * ratio;
    }
    else {
        *out_x = 0.0f;
        *out_y = 0.0f;
    }
}

static DP_LiquifyOpFn liquify_op_get(DP_LiquifyOpType type)
{
    switch (type) {
    case DP_LIQUIFY_OP_TYPE_MOVE:
        return op_move;
    case DP_LIQUIFY_OP_TYPE_SCALE:
        return op_scale;
    case DP_LIQUIFY_OP_TYPE_ROTATE:
        return op_rotate;
    case DP_LIQUIFY_OP_TYPE_SMOOTHE:
        return op_smoothe;
    case DP_LIQUIFY_OP_TYPE_ERASE:
        return op_erase;
    }
    DP_UNREACHABLE();
}

bool DP_liquify_op(DP_Liquify *l, DP_DrawContext *dc,
                   const DP_LiquifyOpParams *params)
{
    DP_ASSERT(l);
    DP_ASSERT(DP_atomic_get(&l->refcount) > 0);
    DP_ASSERT(dc);
    DP_ASSERT(params);

    float center_x = params->x;
    float center_y = params->y;
    float radius = params->radius;

    int left = DP_float_to_int(roundf(center_x - radius));
    int right = DP_float_to_int(roundf(center_x + radius));
    int top = DP_float_to_int(roundf(center_y - radius));
    int bottom = DP_float_to_int(roundf(center_y + radius));
    DP_Rect area = {left, top, right, bottom};
    if (DP_rect_empty(area)) {
        return false;
    }

    int width = DP_rect_width(area);
    int height = DP_rect_height(area);
    int col = left / DP_TILE_SIZE;
    int row = top / DP_TILE_SIZE;
    int xd = left - col * DP_TILE_SIZE;
    int yd = top - row * DP_TILE_SIZE;
    DP_Rect tile_bounds =
        DP_rect_make(col, row, DP_tile_size_round_up(width + xd),
                     DP_tile_size_round_up(height + yd));

    size_t area_size = DP_int_to_size(width) * DP_int_to_size(height);
    float *xs =
        DP_draw_context_pool_require(dc, area_size * sizeof(*xs) * (size_t)2);
    float *ys = xs + area_size;

    float radius_squared = DP_square_float(radius);
    DP_LiquifyOpFn op_fn = liquify_op_get(params->type);

    // Modifications may only be made from one thread, so no need to lock yet.
    DP_LiquifyMap *old_lm = l->lm;
    int out_index = 0;
    for (int y = top; y <= bottom; ++y) {
        float yf = DP_int_to_float(y);
        float dy = yf - center_y;
        float dy_squared = DP_square_float(dy);

        for (int x = left; x <= right; ++x) {
            float xf = DP_int_to_float(x);
            float dx = xf - center_x;
            float dx_squared = DP_square_float(dx);
            float distance_squared = dx_squared + dy_squared;

            float out_x, out_y;
            if (distance_squared < radius_squared) {
                op_fn(params, old_lm, x, y, distance_squared, &out_x, &out_y);
            }
            else {
                map_get_pixel_nullable(old_lm, x, y, &out_x, &out_y);
            }

            xs[out_index] = out_x;
            ys[out_index] = out_y;
            ++out_index;
        }
    }

    DP_Mutex *mutex = l->mutex;
    DP_MUTEX_MUST_LOCK(mutex);

    DP_LiquifyMap *lm;
    if (old_lm) {
        lm = map_make_editable(old_lm,
                               DP_rect_union(tile_bounds, old_lm->tile_bounds));
    }
    else {
        lm = map_alloc_zeroed(tile_bounds);
    }

    DP_TileIterator ti = DP_tile_iterator_make(UINT16_MAX, UINT16_MAX, area);
    while (DP_tile_iterator_next(&ti)) {
        DP_LiquifyTile *lt =
            tile_make_editable_nullable(l, map_get_tile(lm, ti.col, ti.row));

        DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
        while (DP_tile_into_dst_iterator_next(&tidi)) {
            int index = tidi.dst_y * width + tidi.dst_x;
            tile_set_xy(lt, tidi.tile_x, tidi.tile_y, xs[index], ys[index]);
        }

        map_set_tile_noinc(l, lm, ti.col, ti.row, lt);
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


static uint32_t liquify_image_get_checked(const DP_LiquifyImage *li, int x,
                                          int y)
{
    DP_ASSERT(li);
    int width = li->width;
    if (x >= 0 && y >= 0 && x < width && y < li->height) {
        return li->data[y * width + x];
    }
    else {
        return 0;
    }
}

static void liquify_image_set(const DP_LiquifyImage *li, int x, int y,
                              uint32_t value)
{
    DP_ASSERT(li);
    int width = li->width;
    if (x >= 0 && y >= 0 && x < width && y < li->height) {
        DP_ASSERT(x >= 0);
        DP_ASSERT(x < width);
        DP_ASSERT(y >= 0);
        DP_ASSERT(y < li->height);
        li->data[y * width + x] = value;
    }
    else {
        DP_warn("Invalid index to set %d %d in %d,%d %dx%d", x, y, li->x, li->y,
                li->width, li->height);
    }
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

static bool liquify_transformer_apply(DP_LiquifyTransformer *ltr,
                                      DP_LiquifyState *ls, int interpolation)
{
    // TODO diff with old state.
    // FIXME this is wrong, need to collect bounds.

    int source_x = ltr->source.x;
    int source_y = ltr->source.y;
    int source_width = ltr->source.width;
    int source_height = ltr->source.height;
    const uint32_t *source_data = ltr->source.data;
    size_t source_size = DP_int_to_size(source_width)
                       * DP_int_to_size(source_height) * sizeof(*source_data);

    DP_free(ltr->target.data);
    ltr->target.x = source_x;
    ltr->target.y = source_y;
    ltr->target.width = source_width;
    ltr->target.height = source_height;
    ltr->target.data = DP_memdup(ltr->source.data, source_size);

    // FIXME
    int target_to_source_x = 0;
    int target_to_source_y = 0;

    DP_LiquifyMap *lm = ls->lm;
    if (lm) {
        DP_Rect map_bounds = map_pixel_bounds(lm);
        float eps = DP_image_transform_epsilon(interpolation);
        DP_TileIterator ti = DP_tile_iterator_make(
            UINT16_MAX, UINT16_MAX,
            DP_rect_intersection(
                map_bounds,
                DP_rect_make(source_x, source_y, source_width, source_height)));
        while (DP_tile_iterator_next(&ti)) {
            DP_LiquifyTile *lt = map_get_tile(lm, ti.col, ti.row);
            if (lt) {
                DP_TileIntoDstIterator tidi =
                    DP_tile_into_dst_iterator_make(&ti);
                while (DP_tile_into_dst_iterator_next(&tidi)) {
                    int tx = tidi.dst_x + map_bounds.x1 - source_x;
                    int ty = tidi.dst_y + map_bounds.y1 - source_y;
                    int sx = tx + target_to_source_x;
                    int sy = ty + target_to_source_y;
                    float dx, dy;
                    tile_get_xy(lt, tidi.tile_x, tidi.tile_y, &dx, &dy);
                    if (fabsf(dx) < eps && fabsf(dy) < eps) {
                        liquify_image_set(
                            &ltr->target, tx, ty,
                            liquify_image_get_checked(&ltr->source, sx, sy));
                    }
                    else {
                        liquify_image_set(
                            &ltr->target, tx, ty,
                            DP_image_transform_fetch(
                                interpolation, source_width, source_height,
                                source_data,
                                DP_int_to_double(sx) - DP_float_to_double(dx),
                                DP_int_to_double(sy) - DP_float_to_double(dy)));
                    }
                }
            }
        }
    }

    DP_LiquifyState *old_ls = ltr->ls;
    ltr->ls = DP_liquify_state_incref(ls);
    DP_liquify_state_decref_nullable(old_ls);
    return true;
}

bool DP_liquify_transformer_apply(DP_LiquifyTransformer *ltr,
                                  DP_LiquifyState *ls_or_null,
                                  int interpolation)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);

    DP_LiquifyState *old_ls = ltr->ls;
    if (ls_or_null) {
        if (ls_or_null == old_ls) {
            return false;
        }
        else {
            return liquify_transformer_apply(ltr, ls_or_null, interpolation);
        }
    }
    else if (old_ls) {
        DP_liquify_state_decref(old_ls);
        ltr->ls = NULL;
        return true;
    }
    else {
        return false;
    }
}

bool DP_liquify_transformer_target_image(DP_LiquifyTransformer *ltr, int *out_x,
                                         int *out_y, int *out_width,
                                         int *out_height,
                                         const uint32_t **out_data)
{
    DP_ASSERT(ltr);
    DP_ASSERT(DP_atomic_get(&ltr->refcount) > 0);

    if (ltr->target.data) {
        if (out_x) {
            *out_x = ltr->target.x;
        }
        if (out_y) {
            *out_y = ltr->target.y;
        }
        if (out_width) {
            *out_width = ltr->target.width;
        }
        if (out_height) {
            *out_height = ltr->target.height;
        }
        if (out_data) {
            *out_data = ltr->target.data;
        }
        return true;
    }
    else {
        return false;
    }
}
