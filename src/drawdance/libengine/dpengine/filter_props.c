// SPDX-License-Identifier: GPL-3.0-or-later
#include "filter_props.h"
#include "filter_hsx_adjust.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <helpers.h>


struct DP_FilterProps {
    DP_Atomic refcount;
    int type;
    alignas(DP_max_align_t) unsigned char internal[];
};

struct DP_FilterPropsUnknown {
    size_t size;
    int actual_type;
    unsigned char data[];
};

struct DP_FilterPropsHsvAdjust {
    float h, s, v;
};

struct DP_FilterPropsHslAdjust {
    float h, s, l;
};

struct DP_FilterPropsHcyAdjust {
    float h, c, y;
};


static DP_FilterProps *alloc_filter_props(int type, size_t internal_size)
{
    DP_FilterProps *fp = DP_malloc_zeroed(
        DP_FLEX_SIZEOF(DP_FilterProps, internal, internal_size));
    DP_atomic_set(&fp->refcount, 1);
    fp->type = type;
    return fp;
}

static void free_filter_props(DP_FilterProps *fp)
{
    switch (fp->type) {
    case DP_FILTER_TYPE_UNKNOWN:
    case DP_FILTER_TYPE_HSV_ADJUST:
        break;
    }
    DP_free(fp);
}

DP_FilterProps *DP_filter_props_new_unknown(int actual_type,
                                            const unsigned char *data,
                                            size_t size)
{
    DP_FilterProps *fp =
        alloc_filter_props(DP_FILTER_TYPE_UNKNOWN,
                           DP_FLEX_SIZEOF(DP_FilterPropsUnknown, data, size));
    DP_FilterPropsUnknown *fpu = DP_filter_props_internal(fp);
    fpu->size = size;
    fpu->actual_type = actual_type;
    if (size != (size_t)0) {
        memcpy(fpu->data, data, size);
    }
    return fp;
}

DP_FilterProps *DP_filter_props_new_hsv_adjust(float h, float s, float v)
{
    DP_FilterProps *fp = alloc_filter_props(DP_FILTER_TYPE_HSV_ADJUST,
                                            sizeof(DP_FilterPropsHsvAdjust));
    DP_FilterPropsHsvAdjust *fpha = DP_filter_props_internal(fp);
    fpha->h = h;
    fpha->s = s;
    fpha->v = v;
    return fp;
}

DP_FilterProps *DP_filter_props_new_hsl_adjust(float h, float s, float l)
{
    DP_FilterProps *fp = alloc_filter_props(DP_FILTER_TYPE_HSL_ADJUST,
                                            sizeof(DP_FilterPropsHslAdjust));
    DP_FilterPropsHslAdjust *fpha = DP_filter_props_internal(fp);
    fpha->h = h;
    fpha->s = s;
    fpha->l = l;
    return fp;
}

DP_FilterProps *DP_filter_props_new_hcy_adjust(float h, float c, float y)
{
    DP_FilterProps *fp = alloc_filter_props(DP_FILTER_TYPE_HCY_ADJUST,
                                            sizeof(DP_FilterPropsHcyAdjust));
    DP_FilterPropsHcyAdjust *fpha = DP_filter_props_internal(fp);
    fpha->h = h;
    fpha->c = c;
    fpha->y = y;
    return fp;
}


DP_FilterProps *DP_filter_props_incref(DP_FilterProps *fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    DP_atomic_inc(&fp->refcount);
    return fp;
}

DP_FilterProps *DP_filter_props_incref_nullable(DP_FilterProps *fp_or_null)
{
    if (fp_or_null) {
        return DP_filter_props_incref(fp_or_null);
    }
    else {
        return NULL;
    }
}

void DP_filter_props_decref(DP_FilterProps *fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    if (DP_atomic_dec(&fp->refcount)) {
        free_filter_props(fp);
    }
}

void DP_filter_props_decref_nullable(DP_FilterProps *fp_or_null)
{
    if (fp_or_null) {
        DP_filter_props_decref(fp_or_null);
    }
}

int DP_filter_props_refcount(DP_FilterProps *fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    return DP_atomic_get(&fp->refcount);
}


static bool differ_unknown(DP_FilterPropsUnknown *fpu,
                           DP_FilterPropsUnknown *prev_fpu)
{
    size_t size = fpu->size;
    return size != prev_fpu->size
        || memcmp(fpu->data, prev_fpu->data, size) != 0;
}

static bool differ_hsv_adjust(DP_FilterPropsHsvAdjust *fpha,
                              DP_FilterPropsHsvAdjust *prev_fpha)
{
    return fpha->h != prev_fpha->h || fpha->s != prev_fpha->s
        || fpha->v != prev_fpha->v;
}

static bool differ_hsl_adjust(DP_FilterPropsHslAdjust *fpha,
                              DP_FilterPropsHslAdjust *prev_fpha)
{
    return fpha->h != prev_fpha->h || fpha->s != prev_fpha->s
        || fpha->l != prev_fpha->l;
}

static bool differ_hcy_adjust(DP_FilterPropsHcyAdjust *fpha,
                              DP_FilterPropsHcyAdjust *prev_fpha)
{
    return fpha->h != prev_fpha->h || fpha->c != prev_fpha->y
        || fpha->y != prev_fpha->y;
}

static bool filter_props_differ_internal(DP_FilterProps *DP_RESTRICT fp,
                                         DP_FilterProps *DP_RESTRICT prev_fp)
{
    int type = fp->type;
    if (type != prev_fp->type) {
        return true;
    }

    void *internal = DP_filter_props_internal(fp);
    void *prev_internal = DP_filter_props_internal(prev_fp);
    switch (type) {
    case DP_FILTER_TYPE_UNKNOWN:
        return differ_unknown(internal, prev_internal);
    case DP_FILTER_TYPE_HSV_ADJUST:
        return differ_hsv_adjust(internal, prev_internal);
    case DP_FILTER_TYPE_HSL_ADJUST:
        return differ_hsl_adjust(internal, prev_internal);
    case DP_FILTER_TYPE_HCY_ADJUST:
        return differ_hcy_adjust(internal, prev_internal);
    }

    DP_UNREACHABLE();
}

bool DP_filter_props_differ(DP_FilterProps *fp, DP_FilterProps *prev_fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(prev_fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&prev_fp->refcount) > 0);
    if (fp == prev_fp) {
        return false;
    }
    else {
        return filter_props_differ_internal(fp, prev_fp);
    }
}

bool DP_filter_props_differ_nullable(DP_FilterProps *fp_or_null,
                                     DP_FilterProps *prev_fp_or_null)
{
    if (fp_or_null == prev_fp_or_null) {
        return false;
    }
    else if (fp_or_null && prev_fp_or_null) {
        return filter_props_differ_internal(fp_or_null, prev_fp_or_null);
    }
    else {
        return true;
    }
}


static void write_hue_adjust(float x, unsigned char *out)
{
    DP_ASSERT(x >= -1.0f);
    DP_ASSERT(x <= 1.0f);
    DP_write_littleendian_int16(DP_float_to_int16(x * (float)INT16_MAX), out);
}

static float read_hue_adjust(const unsigned char *data)
{
    float f =
        DP_int16_to_float(DP_read_littleendian_int16(data)) / (float)INT16_MAX;
    return CLAMP(f, DP_FILTER_PROPS_ADJUST_ANGLE_MIN,
                 DP_FILTER_PROPS_ADJUST_ANGLE_MAX);
}

static void write_magnitude_adjust(float x, unsigned char *out)
{
    DP_ASSERT(x >= 0.0f);
    DP_ASSERT(x <= 10.0f);
    DP_write_littleendian_uint16(
        DP_float_to_uint16(
            x * ((float)UINT16_MAX / DP_FILTER_PROPS_ADJUST_MAGNITUDE_MAX)),
        out);
}

static float read_magnitude_adjust(const unsigned char *data)
{
    uint16_t x = DP_read_littleendian_uint16(data);
    if (x == 6553) {
        return 1.0f; // Fudge this to be exact.
    }
    else {
        return DP_uint16_to_float(x)
             / ((float)UINT16_MAX / DP_FILTER_PROPS_ADJUST_MAGNITUDE_MAX);
    }
}


#define HEADER_SIZE ((size_t)1)

static size_t serialize_unknown(DP_FilterPropsUnknown *fpu,
                                unsigned char *(*get_buffer)(void *, size_t),
                                void *user)
{
    size_t data_size = fpu->size;
    size_t total_size = HEADER_SIZE + data_size;
    unsigned char *buf = get_buffer(user, total_size);
    if (buf) {
        buf[0] = DP_int_to_uchar(fpu->actual_type);
        if (data_size != 0) {
            memcpy(buf + 1, fpu->data, data_size);
        }
        return total_size;
    }
    else {
        return 0;
    }
}

static size_t serialize_hsv_adjust(DP_FilterPropsHsvAdjust *fpha,
                                   unsigned char *(*get_buffer)(void *, size_t),
                                   void *user)
{
    size_t total_size = HEADER_SIZE + sizeof(int16_t) * (size_t)3;
    unsigned char *buf = get_buffer(user, total_size);
    if (buf) {
        buf[0] = DP_int_to_uchar(DP_FILTER_TYPE_HSV_ADJUST);
        write_hue_adjust(fpha->h, buf + 1);
        write_magnitude_adjust(fpha->s, buf + 3);
        write_magnitude_adjust(fpha->v, buf + 5);
        return total_size;
    }
    else {
        return 0;
    }
}

static size_t serialize_hsl_adjust(DP_FilterPropsHslAdjust *fpha,
                                   unsigned char *(*get_buffer)(void *, size_t),
                                   void *user)
{
    size_t total_size = HEADER_SIZE + sizeof(int16_t) * (size_t)3;
    unsigned char *buf = get_buffer(user, total_size);
    if (buf) {
        buf[0] = DP_int_to_uchar(DP_FILTER_TYPE_HSL_ADJUST);
        write_hue_adjust(fpha->h, buf + 1);
        write_magnitude_adjust(fpha->s, buf + 3);
        write_magnitude_adjust(fpha->l, buf + 5);
        return total_size;
    }
    else {
        return 0;
    }
}

static size_t serialize_hcy_adjust(DP_FilterPropsHcyAdjust *fpha,
                                   unsigned char *(*get_buffer)(void *, size_t),
                                   void *user)
{
    size_t total_size = HEADER_SIZE + sizeof(int16_t) * (size_t)3;
    unsigned char *buf = get_buffer(user, total_size);
    if (buf) {
        buf[0] = DP_int_to_uchar(DP_FILTER_TYPE_HSL_ADJUST);
        write_hue_adjust(fpha->h, buf + 1);
        write_magnitude_adjust(fpha->c, buf + 3);
        write_magnitude_adjust(fpha->y, buf + 5);
        return total_size;
    }
    else {
        return 0;
    }
}

size_t DP_filter_props_serialize(DP_FilterProps *fp,
                                 unsigned char *(*get_buffer)(void *, size_t),
                                 void *user)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);

    void *internal = DP_filter_props_internal(fp);
    switch (fp->type) {
    case DP_FILTER_TYPE_UNKNOWN:
        return serialize_unknown(internal, get_buffer, user);
    case DP_FILTER_TYPE_HSV_ADJUST:
        return serialize_hsv_adjust(internal, get_buffer, user);
    case DP_FILTER_TYPE_HSL_ADJUST:
        return serialize_hsl_adjust(internal, get_buffer, user);
    case DP_FILTER_TYPE_HCY_ADJUST:
        return serialize_hcy_adjust(internal, get_buffer, user);
    }

    DP_UNREACHABLE();
}


static DP_FilterProps *deserialize_hsv_adjust(const unsigned char *data,
                                              size_t size)
{
    if (size == 7) {
        return DP_filter_props_new_hsv_adjust(read_hue_adjust(data + 1),
                                              read_magnitude_adjust(data + 3),
                                              read_magnitude_adjust(data + 5));
    }
    else {
        DP_warn("Invalid size %zu for HSV_ADJUST filter", size);
        return NULL;
    }
}

static DP_FilterProps *deserialize_hsl_adjust(const unsigned char *data,
                                              size_t size)
{
    if (size == 7) {
        return DP_filter_props_new_hsl_adjust(read_hue_adjust(data + 1),
                                              read_magnitude_adjust(data + 3),
                                              read_magnitude_adjust(data + 5));
    }
    else {
        DP_warn("Invalid size %zu for HSL_ADJUST filter", size);
        return NULL;
    }
}

static DP_FilterProps *deserialize_hcy_adjust(const unsigned char *data,
                                              size_t size)
{
    if (size == 7) {
        return DP_filter_props_new_hcy_adjust(read_hue_adjust(data + 1),
                                              read_magnitude_adjust(data + 3),
                                              read_magnitude_adjust(data + 5));
    }
    else {
        DP_warn("Invalid size %zu for HCY_ADJUST filter", size);
        return NULL;
    }
}

static DP_FilterProps *
deserialize_with(int type, const unsigned char *data, size_t size,
                 DP_FilterProps *(*fn)(const unsigned char *, size_t))
{
    DP_FilterProps *fp = fn(data, size);
    if (fp) {
        return fp;
    }
    else {
        return DP_filter_props_new_unknown(type, data, size);
    }
}

DP_FilterProps *DP_filter_props_deserialize(const unsigned char *data,
                                            size_t size)
{
    DP_ASSERT(data);
    DP_ASSERT(size != 0);

    int type = data[0];
    switch (type) {
    case 0:
    case DP_FILTER_TYPE_UNKNOWN:
        return DP_filter_props_new_unknown(DP_FILTER_TYPE_UNKNOWN, data, size);
    case DP_FILTER_TYPE_HSV_ADJUST:
        return deserialize_with(type, data, size, deserialize_hsv_adjust);
    case DP_FILTER_TYPE_HSL_ADJUST:
        return deserialize_with(type, data, size, deserialize_hsl_adjust);
    case DP_FILTER_TYPE_HCY_ADJUST:
        return deserialize_with(type, data, size, deserialize_hcy_adjust);
    }

    DP_warn("Unknown filter type %d", type);
    return DP_filter_props_new_unknown(type, data, size);
}


int DP_filter_props_type(DP_FilterProps *fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    return fp->type;
}

void *DP_filter_props_internal(DP_FilterProps *fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    return fp->internal;
}

static bool effective_hsv_adjust(DP_FilterPropsHsvAdjust *fpha)
{
    return fpha->h != 0.0f || fpha->s != 1.0f || fpha->v != 1.0f;
}

static bool effective_hsl_adjust(DP_FilterPropsHslAdjust *fpha)
{
    return fpha->h != 0.0f || fpha->s != 1.0f || fpha->l != 1.0f;
}

static bool effective_hcy_adjust(DP_FilterPropsHcyAdjust *fpha)
{
    return fpha->h != 0.0f || fpha->c != 1.0f || fpha->y != 1.0f;
}

bool DP_filter_props_effective(DP_FilterProps *fp)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    switch (fp->type) {
    case DP_FILTER_TYPE_HSV_ADJUST:
        return effective_hsv_adjust(DP_filter_props_internal(fp));
    case DP_FILTER_TYPE_HSL_ADJUST:
        return effective_hsl_adjust(DP_filter_props_internal(fp));
    case DP_FILTER_TYPE_HCY_ADJUST:
        return effective_hcy_adjust(DP_filter_props_internal(fp));
    default:
        return false;
    }
}

void DP_filter_props_apply_tile(DP_FilterProps *fp, DP_TransientTile *tt,
                                DP_Tile *t)
{
    DP_ASSERT(fp);
    DP_ASSERT(DP_atomic_get(&fp->refcount) > 0);
    DP_ASSERT(tt);
    DP_ASSERT(t);
    switch (fp->type) {
    case DP_FILTER_TYPE_HSV_ADJUST:
        DP_filter_tile_hsv_adjust(DP_filter_props_internal(fp), tt, t);
        break;
    case DP_FILTER_TYPE_HSL_ADJUST:
        DP_filter_tile_hsl_adjust(DP_filter_props_internal(fp), tt, t);
        break;
    case DP_FILTER_TYPE_HCY_ADJUST:
        DP_filter_tile_hcy_adjust(DP_filter_props_internal(fp), tt, t);
        break;
    default:
        DP_warn("Unhandled filter %d", fp->type);
        break;
    }
}


float DP_filter_props_hsv_adjust_h(DP_FilterPropsHsvAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->h;
}

float DP_filter_props_hsv_adjust_s(DP_FilterPropsHsvAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->s;
}

float DP_filter_props_hsv_adjust_v(DP_FilterPropsHsvAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->v;
}


float DP_filter_props_hsl_adjust_h(DP_FilterPropsHslAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->h;
}

float DP_filter_props_hsl_adjust_s(DP_FilterPropsHslAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->s;
}

float DP_filter_props_hsl_adjust_l(DP_FilterPropsHslAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->l;
}


float DP_filter_props_hcy_adjust_h(DP_FilterPropsHcyAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->h;
}

float DP_filter_props_hcy_adjust_c(DP_FilterPropsHcyAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->c;
}

float DP_filter_props_hcy_adjust_y(DP_FilterPropsHcyAdjust *fpha)
{
    DP_ASSERT(fpha);
    return fpha->y;
}
