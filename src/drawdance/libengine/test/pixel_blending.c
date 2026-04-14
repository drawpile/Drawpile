// SPDX-License-Identifier: MIT
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/cpu.h>
#include <dpcommon/output.h>
#include <dpengine/pixels.h>
#include <dpengine/tile.h>
#include <dpmsg/blend_mode.h>
#include <dptest.h>

static int get_runs_from_environment(const char *key)
{
    const char *value = getenv(key);
    if (value) {
        int runs = atoi(value);
        if (runs > 0) {
            return runs;
        }
    }
    return 10;
}

static bool blend_mode_has_cpu_support(
#ifndef DP_CPU_X64
    DP_UNUSED
#endif
    int blend_mode,
    DP_CpuSupport cpu_support)
{
    switch (cpu_support) {
#ifdef DP_CPU_X64
    case DP_CPU_SUPPORT_SSE42:
    case DP_CPU_SUPPORT_AVX2:
        switch (blend_mode) {
        case DP_BLEND_MODE_NORMAL:
        case DP_BLEND_MODE_RECOLOR:
        case DP_BLEND_MODE_BEHIND:
        case DP_BLEND_MODE_NORMAL_AND_ERASER:
        case DP_BLEND_MODE_PIGMENT:
        case DP_BLEND_MODE_PIGMENT_ALPHA:
        case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        case DP_BLEND_MODE_OKLAB_NORMAL:
        case DP_BLEND_MODE_OKLAB_RECOLOR:
        case DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER:
            return true;
        default:
            return false;
        }
#endif
    default:
        return false;
    }
}

static uint16_t generate_fix15(void)
{
    return DP_int_to_uint16(rand() % DP_BIT15);
}

static uint16_t generate_fix15_bounded(int max_inclusive)
{
    return DP_int_to_uint16(rand() % (max_inclusive + 1));
}

static DP_Pixel15 generate_pixel15(void)
{
    uint16_t a = generate_fix15();
    return (DP_Pixel15){
        .b = generate_fix15_bounded(a),
        .g = generate_fix15_bounded(a),
        .r = generate_fix15_bounded(a),
        .a = a,
    };
}

static DP_UPixel15 generate_upixel15(void)
{
    return (DP_UPixel15){
        .b = generate_fix15(),
        .g = generate_fix15(),
        .r = generate_fix15(),
        .a = generate_fix15(),
    };
}

static DP_Pixel15 *allocate_tile_pixels(void)
{
    return DP_malloc_simd(DP_TILE_BYTES);
}

static void copy_tile_pixels(DP_Pixel15 *DP_RESTRICT out_pixels,
                             const DP_Pixel15 *DP_RESTRICT in_pixels)
{
    memcpy(out_pixels, in_pixels, DP_TILE_BYTES);
}

static bool pixel_channels_in_bounds(DP_Pixel15 pixel)
{
    return pixel.a <= DP_BIT15 && pixel.b <= pixel.a && pixel.g <= pixel.a
        && pixel.r <= pixel.a;
}

static void diag_pixel(TEST_PARAMS, const char *title, DP_Pixel15 pixel)
{
    DIAG("%s: b%8u, g%8u, r%8u, a%8u", title, pixel.b, pixel.g, pixel.r,
         pixel.a);
}

static void diag_upixel(TEST_PARAMS, const char *title, DP_UPixel15 pixel)
{
    DIAG("%s: b%8u, g%8u, r%8u, a%8u", title, pixel.b, pixel.g, pixel.r,
         pixel.a);
}

static void diag_mask(TEST_PARAMS, const char *title, uint16_t value)
{
    DIAG("%s: %8u", title, value);
}

static bool check_pixels_in_bounds(TEST_PARAMS, int blend_mode,
                                   const DP_Pixel15 *DP_RESTRICT dst,
                                   const DP_Pixel15 *DP_RESTRICT src,
                                   const DP_Pixel15 *DP_RESTRICT result)
{
    bool ok = true;
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        if (!pixel_channels_in_bounds(result[i])) {
            ok = false;
            DIAG("blend pixel CPU %d %s at %d out of bounds",
                 (int)DP_cpu_support,
                 DP_blend_mode_enum_name_unprefixed(blend_mode), i);
            diag_pixel(TEST_ARGS, "dst", dst[i]);
            diag_pixel(TEST_ARGS, "src", src[i]);
            diag_pixel(TEST_ARGS, "res", result[i]);
        }
    }
    return ok;
}

static bool check_mask_in_bounds(TEST_PARAMS, int blend_mode,
                                 const DP_Pixel15 *DP_RESTRICT dst,
                                 DP_UPixel15 src, const uint16_t *mask,
                                 const DP_Pixel15 *DP_RESTRICT result)
{
    bool ok = true;
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        if (!pixel_channels_in_bounds(result[i])) {
            ok = false;
            DIAG("blend mask CPU %d %s at %d out of bounds",
                 (int)DP_cpu_support,
                 DP_blend_mode_enum_name_unprefixed(blend_mode), i);
            diag_pixel(TEST_ARGS, "dst", dst[i]);
            diag_upixel(TEST_ARGS, "src", src);
            diag_mask(TEST_ARGS, "msk", mask[i]);
            diag_pixel(TEST_ARGS, "res", result[i]);
        }
    }
    return ok;
}

static bool channel_fuzzy_equal(uint16_t a, uint16_t b)
{
    // Floating point inaccuracies can cause small differences in channel
    // values. This doesn't matter in practice, since the 15 bit channels only
    // get displayed and persisted with 8 bit precision. And even those don't
    // need to be totally accurate to be visually indistinguishable, so on a
    // real canvas the error never accumulates to a significant degree.
    uint16_t delta = a < b ? b - a : a - b;
    return delta < 32;
}

static bool pixel_fuzzy_equal(DP_Pixel15 a, DP_Pixel15 b)
{
    return channel_fuzzy_equal(a.b, b.b) && channel_fuzzy_equal(a.g, b.g)
        && channel_fuzzy_equal(a.r, b.r) && channel_fuzzy_equal(a.a, b.a);
}

static bool check_pixels_match(TEST_PARAMS, int blend_mode,
                               const DP_Pixel15 *DP_RESTRICT dst,
                               const DP_Pixel15 *DP_RESTRICT src,
                               const DP_Pixel15 *DP_RESTRICT expected_result,
                               const DP_Pixel15 *DP_RESTRICT actual_result)
{
    bool ok = true;
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        if (!pixel_fuzzy_equal(expected_result[i], actual_result[i])) {
            ok = false;
            DIAG("blend tile CPU %d %s at %d mismatch", (int)DP_cpu_support,
                 DP_blend_mode_enum_name_unprefixed(blend_mode), i);
            diag_pixel(TEST_ARGS, "dst", dst[i]);
            diag_pixel(TEST_ARGS, "src", src[i]);
            diag_pixel(TEST_ARGS, "wnt", expected_result[i]);
            diag_pixel(TEST_ARGS, "got", actual_result[i]);
        }
    }
    return ok;
}

static bool check_mask_match(TEST_PARAMS, int blend_mode,
                             const DP_Pixel15 *DP_RESTRICT dst, DP_UPixel15 src,
                             const uint16_t *mask,
                             const DP_Pixel15 *DP_RESTRICT expected_result,
                             const DP_Pixel15 *DP_RESTRICT actual_result)
{
    bool ok = true;
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        if (!pixel_fuzzy_equal(expected_result[i], actual_result[i])) {
            ok = false;
            DIAG("blend tile CPU %d %s at %d mismatch", (int)DP_cpu_support,
                 DP_blend_mode_enum_name_unprefixed(blend_mode), i);
            diag_pixel(TEST_ARGS, "dst", dst[i]);
            diag_upixel(TEST_ARGS, "src", src);
            diag_mask(TEST_ARGS, "msk", mask[i]);
            diag_pixel(TEST_ARGS, "wnt", expected_result[i]);
            diag_pixel(TEST_ARGS, "got", actual_result[i]);
        }
    }
    return ok;
}


static bool blend_tile_with(TEST_PARAMS, const DP_Pixel15 *DP_RESTRICT dst,
                            const DP_Pixel15 *DP_RESTRICT src,
                            DP_Pixel15 *DP_RESTRICT default_result,
                            DP_Pixel15 *DP_RESTRICT simd_result,
                            uint16_t opacity, int blend_mode)
{
    bool ok = true;

    DP_cpu_support_set(DP_CPU_SUPPORT_DEFAULT);
    DP_CpuSupport initial_cpu_support = DP_cpu_support;

    copy_tile_pixels(default_result, dst);
    DP_blend_tile(default_result, src, opacity, blend_mode);
    if (!check_pixels_in_bounds(TEST_ARGS, blend_mode, dst, src,
                                default_result)) {
        ok = false;
    }

    for (int i = (int)DP_CPU_SUPPORT_DEFAULT + 1; i < (int)DP_CPU_SUPPORT_COUNT;
         ++i) {
        DP_CpuSupport cpu_support = (DP_CpuSupport)i;
        bool should_test = cpu_support != initial_cpu_support
                        && blend_mode_has_cpu_support(blend_mode, cpu_support)
                        && DP_cpu_support_set(cpu_support);
        if (should_test) {
            copy_tile_pixels(simd_result, dst);
            DP_blend_tile(simd_result, src, opacity, blend_mode);
            if (!check_pixels_in_bounds(TEST_ARGS, blend_mode, dst, src,
                                        simd_result)) {
                ok = false;
            }
            else if (!check_pixels_match(TEST_ARGS, blend_mode, dst, src,
                                         default_result, simd_result)) {
                ok = false;
            }
        }
    }

    return ok;
}

static void blend_tile(TEST_PARAMS)
{
    DP_Pixel15 *dst = allocate_tile_pixels();
    DP_Pixel15 *src = allocate_tile_pixels();
    DP_Pixel15 *default_result = allocate_tile_pixels();
    DP_Pixel15 *simd_result = allocate_tile_pixels();

    int runs = get_runs_from_environment("DP_TEST_PIXEL_BLENDING_TILE_RUNS");
    for (int run = 0; run < runs; ++run) {
        uint16_t opacity = generate_fix15();
        for (int i = 0; i < DP_TILE_LENGTH; ++i) {
            dst[i] = generate_pixel15();
            src[i] = generate_pixel15();
        }

        for (int blend_mode = 0; blend_mode < DP_BLEND_MODE_COUNT;
             ++blend_mode) {
            if (DP_blend_mode_valid_for_layer(blend_mode)) {
                OK(blend_tile_with(TEST_ARGS, dst, src, default_result,
                                   simd_result, opacity, blend_mode),
                   "blend tile with %s at opacity %d",
                   DP_blend_mode_enum_name_unprefixed(blend_mode),
                   DP_uint16_to_int(opacity));
            }
        }
    }

    DP_free_simd(simd_result);
    DP_free_simd(default_result);
    DP_free_simd(dst);
    DP_free_simd(src);
}


static bool blend_mask_with(TEST_PARAMS, const DP_Pixel15 *DP_RESTRICT dst,
                            DP_UPixel15 src, const uint16_t *mask,
                            DP_Pixel15 *DP_RESTRICT default_result,
                            DP_Pixel15 *DP_RESTRICT simd_result,
                            uint16_t opacity, int blend_mode)
{
    bool ok = true;

    DP_cpu_support_set(DP_CPU_SUPPORT_DEFAULT);
    DP_CpuSupport initial_cpu_support = DP_cpu_support;

    copy_tile_pixels(default_result, dst);
    DP_blend_mask(default_result, src, blend_mode, mask, opacity, DP_TILE_SIZE,
                  DP_TILE_SIZE, 0, 0);
    if (!check_mask_in_bounds(TEST_ARGS, blend_mode, dst, src, mask,
                              default_result)) {
        ok = false;
    }

    for (int i = (int)DP_CPU_SUPPORT_DEFAULT + 1; i < (int)DP_CPU_SUPPORT_COUNT;
         ++i) {
        DP_CpuSupport cpu_support = (DP_CpuSupport)i;
        bool should_test = cpu_support != initial_cpu_support
                        && blend_mode_has_cpu_support(blend_mode, cpu_support)
                        && DP_cpu_support_set(cpu_support);
        if (should_test) {
            copy_tile_pixels(simd_result, dst);
            DP_blend_mask(simd_result, src, blend_mode, mask, opacity,
                          DP_TILE_SIZE, DP_TILE_SIZE, 0, 0);
            if (!check_mask_in_bounds(TEST_ARGS, blend_mode, dst, src, mask,
                                      simd_result)) {
                ok = false;
            }
            else if (!check_mask_match(TEST_ARGS, blend_mode, dst, src, mask,
                                       default_result, simd_result)) {
                ok = false;
            }
        }
    }

    return ok;
}

static void blend_mask(TEST_PARAMS)
{
    DP_Pixel15 *dst = allocate_tile_pixels();
    uint16_t *mask = DP_malloc_simd(DP_TILE_LENGTH * sizeof(*mask));
    DP_Pixel15 *default_result = allocate_tile_pixels();
    DP_Pixel15 *simd_result = allocate_tile_pixels();

    int runs = get_runs_from_environment("DP_TEST_PIXEL_BLENDING_MASK_RUNS");
    for (int run = 0; run < runs; ++run) {
        DP_UPixel15 src = generate_upixel15();
        uint16_t opacity = generate_fix15();
        for (int i = 0; i < DP_TILE_LENGTH; ++i) {
            dst[i] = generate_pixel15();
            mask[i] = generate_fix15();
        }

        for (int blend_mode = 0; blend_mode < DP_BLEND_MODE_COUNT;
             ++blend_mode) {
            if (DP_blend_mode_valid_for_brush(blend_mode)) {
                OK(blend_mask_with(TEST_ARGS, dst, src, mask, default_result,
                                   simd_result, opacity, blend_mode),
                   "blend mask with %s at opacity %d",
                   DP_blend_mode_enum_name_unprefixed(blend_mode),
                   DP_uint16_to_int(opacity));
            }
        }
    }

    DP_free_simd(simd_result);
    DP_free_simd(default_result);
    DP_free_simd(mask);
    DP_free_simd(dst);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(blend_tile);
    REGISTER_TEST(blend_mask);
}

int main(int argc, char **argv)
{
    DP_test_main(argc, argv, register_tests, NULL);
}
