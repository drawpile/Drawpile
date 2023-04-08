#include "bench_common.hpp"

extern "C" {
#include "dpcommon/cpu.h"
#include "dpcommon/input.h"
#include "dpcommon/output.h"
#include "dpcommon/perf.h"
#include "dpengine/image.h"
#include "dpengine/pixels.h"
#include "dpengine/tile.h"
#include "dpmsg/blend_mode.h"
}

#include <benchmark/benchmark.h>

#pragma region sse
DP_TARGET_BEGIN("sse4.2")
static void blend_pixels_normal_sse42(DP_Pixel15 *dst, DP_Pixel15 *src,
                                      int pixel_count, uint16_t opacity)
{
    // clang-format off
    __m128i o = _mm_set1_epi32(opacity); // o = opactity

    // 4 pixels are loaded at a time
    for (int i = 0; i < pixel_count; i += 4) {
        // load
        __m128i srcB, srcG, srcR, srcA;
        load_aligned_sse42(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m128i dstB, dstG, dstR, dstA;
        load_aligned_sse42(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Normal blend
        __m128i srcAO = mul_sse42(srcA, o);
        __m128i as1 = _mm_sub_epi32(_mm_set1_epi32(DP_BIT15), srcAO); // as1 = DP_BIT15 - srcA * o

        dstB = _mm_add_epi32(mul_sse42(dstB, as1), mul_sse42(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm_add_epi32(mul_sse42(dstG, as1), mul_sse42(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm_add_epi32(mul_sse42(dstR, as1), mul_sse42(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm_add_epi32(mul_sse42(dstA, as1), srcAO);              // dstA = (dstA * as1) + (srcA * o)

        // store
        store_aligned_sse42(dstB, dstG, dstR, dstA, &dst[i]);
    }
    // clang-format on
}
DP_TARGET_END
#pragma endregion

#pragma region avx
DP_TARGET_BEGIN("avx2")
static void blend_pixels_normal_avx2(DP_Pixel15 *dst, DP_Pixel15 *src,
                                     int pixel_count, uint16_t opacity)
{
    // clang-format off
    __m256i o = _mm256_set1_epi32(opacity); // o = opactity

    // 4 pixels are loaded at a time
    for (int i = 0; i < pixel_count; i += 8) {
        // load
        __m256i srcB, srcG, srcR, srcA;
        load_aligned_avx2(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m256i dstB, dstG, dstR, dstA;
        load_aligned_avx2(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Normal blend
        __m256i srcAO = mul_avx2(srcA, o);
        __m256i as1 = _mm256_sub_epi32(_mm256_set1_epi32(1 << 15), srcAO); // as1 = 1 - srcA * o

        dstB = _mm256_add_epi32(mul_avx2(dstB, as1), mul_avx2(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm256_add_epi32(mul_avx2(dstG, as1), mul_avx2(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm256_add_epi32(mul_avx2(dstR, as1), mul_avx2(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm256_add_epi32(mul_avx2(dstA, as1), srcAO);             // dstA = (dstA * as1) + (srcA * o)

        // store
        store_aligned_avx2(dstB, dstG, dstR, dstA, &dst[i]);
    }
    // clang-format on
}
DP_TARGET_END
#pragma endregion

#pragma region seq
static void blend_pixels_normal_seq(DP_Pixel15 *dst, DP_Pixel15 *src,
                                    int pixel_count, uint16_t o)
{
    for (int i = 0; i < pixel_count; i++) {
        uint32_t as1 = DP_BIT15 - mul_seq(src[i].a, o);

        dst[i].b = mul_seq(dst[i].b, as1) + mul_seq(src[i].b, o);
        dst[i].g = mul_seq(dst[i].g, as1) + mul_seq(src[i].g, o);
        dst[i].r = mul_seq(dst[i].r, as1) + mul_seq(src[i].r, o);
        dst[i].a = mul_seq(dst[i].a, as1) + mul_seq(src[i].a, o);
    }
}
#pragma endregion

static void blend_pixels_normal_cpu_support_branching(DP_Pixel15 *dst,
                                                      DP_Pixel15 *src,
                                                      int pixel_count,
                                                      uint16_t opacity)
{
    DP_CpuSupport cpu_support = DP_cpu_support;

    if (cpu_support >= DP_CPU_SUPPORT_AVX2) {
        int remaining = pixel_count % 8;
        int count = pixel_count - remaining;

        blend_pixels_normal_avx2(dst, src, count, opacity);

        src += count;
        dst += count;
        pixel_count = remaining;
    }

    if (cpu_support >= DP_CPU_SUPPORT_SSE42) {
        int remaining = pixel_count % 4;
        int count = pixel_count - remaining;

        blend_pixels_normal_sse42(dst, src, count, opacity);

        src += count;
        dst += count;
        pixel_count = remaining;
    }

    blend_pixels_normal_seq(dst, src, pixel_count, opacity);
}

#pragma region ptr

static void blend_pixels_normal_cpu_support_ptr_select(DP_Pixel15 *dst,
                                                       DP_Pixel15 *src,
                                                       int pixel_count,
                                                       uint16_t opacity);
void (*ptr)(DP_Pixel15 *dst, DP_Pixel15 *src, int pixel_count,
            uint16_t opacity) = blend_pixels_normal_cpu_support_ptr_select;

static void blend_pixels_normal_cpu_support_ptr_select(DP_Pixel15 *dst,
                                                       DP_Pixel15 *src,
                                                       int pixel_count,
                                                       uint16_t opacity)
{
    DP_CpuSupport cpu_support = DP_cpu_support;

    if (cpu_support >= DP_CPU_SUPPORT_AVX2) {
        ptr = blend_pixels_normal_avx2;
    }
    else if (cpu_support >= DP_CPU_SUPPORT_SSE42) {
        ptr = blend_pixels_normal_sse42;
    }
    else {
        ptr = blend_pixels_normal_seq;
    }

    ptr(dst, src, pixel_count, opacity);
}


static void blend_pixels_normal_cpu_support_ptr(DP_Pixel15 *dst,
                                                DP_Pixel15 *src,
                                                int pixel_count,
                                                uint16_t opacity)
{
    ptr(dst, src, pixel_count, opacity);
}
#pragma endregion

static void nop_(DP_Pixel15 *dst, DP_Pixel15 *src, int pixel_count,
                 uint16_t opacity)
{
}

#define BENCH_BLEND(name, func)                                       \
    static void name(benchmark::State &state)                         \
    {                                                                 \
        DP_Pixel15 *dst = (DP_Pixel15 *)DP_malloc_simd_zeroed(        \
            DP_TILE_LENGTH * sizeof(DP_Pixel15));                     \
        DP_Pixel15 *src = (DP_Pixel15 *)DP_malloc_simd_zeroed(        \
            DP_TILE_LENGTH * sizeof(DP_Pixel15));                     \
                                                                      \
        for (auto _ : state) {                                        \
            benchmark::DoNotOptimize(dst);                            \
            benchmark::DoNotOptimize(src);                            \
                                                                      \
            no_inline_func(func, dst, src, DP_TILE_LENGTH, DP_BIT15); \
                                                                      \
            benchmark::DoNotOptimize(dst);                            \
        }                                                             \
                                                                      \
        DP_free_simd(dst);                                            \
        DP_free_simd(src);                                            \
    }                                                                 \
    BENCHMARK(name)

BENCH_BLEND(nop, nop_);
BENCH_BLEND(raw_sse42, blend_pixels_normal_sse42);
BENCH_BLEND(raw_avx2, blend_pixels_normal_avx2);
BENCH_BLEND(raw_seq, blend_pixels_normal_seq);
BENCH_BLEND(cpu_support_branching, blend_pixels_normal_cpu_support_branching);
BENCH_BLEND(cpu_support_ptr, blend_pixels_normal_cpu_support_ptr);

BENCHMARK_MAIN();
