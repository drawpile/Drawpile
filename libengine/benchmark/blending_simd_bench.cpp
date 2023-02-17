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
// Load 4 16bit pixels and split them into 4x32 bit registers.
DP_FORCE_INLINE void load_sse42(DP_Pixel15 src[4], __m128i *out_blue,
                                __m128i *out_green, __m128i *out_red,
                                __m128i *out_alpha)
{
    // clang-format off
    DP_ASSERT(((intptr_t)src) % 16 == 0);

    __m128i source1 = _mm_load_si128((__m128i *)src);     // |B1|G1|R1|A1|B2|G2|R2|A2|
    __m128i source2 = _mm_load_si128((__m128i *)src + 1); // |B3|G3|R3|A3|B4|G4|R4|A4|

    __m128i shuffled1 = _mm_shuffle_epi32(source1, _MM_SHUFFLE(3, 1, 2, 0)); // |B1|G1|B2|G2|R1|A1|R2|A2|
    __m128i shuffled2 = _mm_shuffle_epi32(source2, _MM_SHUFFLE(3, 1, 2, 0)); // |B3|G3|B4|G4|R3|A3|R3|A3|

    __m128i blue_green = _mm_unpacklo_epi64(shuffled1, shuffled2); // |B1|G1|B2|G2|B3|G3|B4|G4|
    __m128i red_alpha = _mm_unpackhi_epi64(shuffled1, shuffled2);  // |R1|A1|R2|A2|R3|A3|R4|A4|

    *out_blue = _mm_blend_epi16(blue_green, _mm_set1_epi32(0), 170); // Zero out upper bits (16 -> 32)
    *out_green = _mm_srli_epi32(blue_green, 16);                     // Move and zero out upper bits (16 -> 32)
    *out_red = _mm_blend_epi16(blue_green, _mm_set1_epi32(0), 170);  // Zero out upper bits (16 -> 32)
    *out_alpha = _mm_srli_epi32(red_alpha, 16);                      // Move and zero out upper bits (16 -> 32)
    // clang-format on
}

// Store 4x32 bit registers into 4 16bit pixels.
DP_FORCE_INLINE void store_sse42(__m128i blue, __m128i green, __m128i red,
                                 __m128i alpha, DP_Pixel15 dest[4])
{
    // clang-format off
    DP_ASSERT(((intptr_t)dest) % 16 == 0);

    __m128i blue_green = _mm_blend_epi16(blue, _mm_slli_si128(green, 2), 170); // |B1|G1|B2|G2|B3|G3|B4|G4|
    __m128i red_alpha = _mm_blend_epi16(red, _mm_slli_si128(alpha, 2), 170);   // |R1|A1|R2|A2|R3|A3|R4|A4|

    __m128i lo = _mm_unpacklo_epi32(blue_green, red_alpha); // |B1|G1|R1|A1|B2|G2|R2|A2|
    __m128i hi = _mm_unpackhi_epi32(blue_green, red_alpha); // |B3|G3|R3|A3|B4|G4|R4|A4|

    _mm_store_si128((__m128i *)dest, lo);
    _mm_store_si128((__m128i *)dest + 1, hi);
    // clang-format on
}

DP_FORCE_INLINE __m128i mul_sse42(__m128i a, __m128i b)
{
    return _mm_srli_epi32(_mm_mullo_epi32(a, b), 15);
}

static void blend_pixels_normal_sse42(DP_Pixel15 *dst, DP_Pixel15 *src,
                                      int pixel_count, uint16_t opacity)
{
    // clang-format off
    __m128i o = _mm_set1_epi32(opacity); // o = opactity

    // 4 pixels are loaded at a time
    for (int i = 0; i < pixel_count; i += 4) {
        // load
        __m128i srcB, srcG, srcR, srcA;
        load_sse42(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m128i dstB, dstG, dstR, dstA;
        load_sse42(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Normal blend
        __m128i srcAO = mul_sse42(srcA, o);
        __m128i as1 = _mm_sub_epi32(_mm_set1_epi32(DP_BIT15), srcAO); // as1 = DP_BIT15 - srcA * o

        dstB = _mm_add_epi32(mul_sse42(dstB, as1), mul_sse42(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm_add_epi32(mul_sse42(dstG, as1), mul_sse42(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm_add_epi32(mul_sse42(dstR, as1), mul_sse42(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm_add_epi32(mul_sse42(dstA, as1), srcAO);              // dstA = (dstA * as1) + (srcA * o)

        // store
        store_sse42(dstB, dstG, dstR, dstA, &dst[i]);
    }
    // clang-format on
}
DP_TARGET_END
#pragma endregion

#pragma region avx
DP_TARGET_BEGIN("avx2")
// Load 8 16bit pixels and split them into 8x32 bit registers.
DP_FORCE_INLINE void load_avx2(DP_Pixel15 src[8], __m256i *out_blue,
                               __m256i *out_green, __m256i *out_red,
                               __m256i *out_alpha)
{
    // clang-format off
    __m256i source1 = _mm256_loadu_si256((__m256i *)src);
    __m256i source2 = _mm256_loadu_si256((__m256i *)src + 1);

    __m256i hi = _mm256_unpackhi_epi32(source1, source2);
    __m256i lo = _mm256_unpacklo_epi32(source1, source2);

    __m256i blue_green = _mm256_unpacklo_epi64(lo, hi);
    __m256i red_alpha = _mm256_unpackhi_epi64(lo, hi);

    *out_blue = _mm256_blend_epi16(blue_green, _mm256_set1_epi32(0), 170);
    *out_green = _mm256_srli_epi32(blue_green, 16);
    *out_red = _mm256_blend_epi16(red_alpha, _mm256_set1_epi32(0), 170);
    *out_alpha = _mm256_srli_epi32(red_alpha, 16);
    // clang-format on
}

// Store 8x32 bit registers into 8 16bit pixels.
DP_FORCE_INLINE void store_avx2(__m256i blue, __m256i green, __m256i red,
                                __m256i alpha, DP_Pixel15 dest[8])
{
    // clang-format off
    __m256i blue_green = _mm256_blend_epi16(blue, _mm256_slli_si256(green, 2), 170);
    __m256i red_alpha = _mm256_blend_epi16(red, _mm256_slli_si256(alpha, 2), 170);

    __m256i hi = _mm256_unpackhi_epi32(blue_green, red_alpha);
    __m256i lo = _mm256_unpacklo_epi32(blue_green, red_alpha);

    __m256i source1 = _mm256_unpacklo_epi64(lo, hi);
    __m256i source2 = _mm256_unpackhi_epi64(lo, hi);

    _mm256_storeu_si256((__m256i *)dest, source1);
    _mm256_storeu_si256((__m256i *)dest + 1, source2);
    // clang-format on
}

DP_FORCE_INLINE __m256i mul_avx2(__m256i a, __m256i b)
{
    return _mm256_srli_epi32(_mm256_mullo_epi32(a, b), 15);
}

static void blend_pixels_normal_avx2(DP_Pixel15 *dst, DP_Pixel15 *src,
                                     int pixel_count, uint16_t opacity)
{
    // clang-format off
    __m256i o = _mm256_set1_epi32(opacity); // o = opactity

    // 4 pixels are loaded at a time
    for (int i = 0; i < pixel_count; i += 8) {
        // load
        __m256i srcB, srcG, srcR, srcA;
        load_avx2(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m256i dstB, dstG, dstR, dstA;
        load_avx2(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Normal blend
        __m256i srcAO = mul_avx2(srcA, o);
        __m256i as1 = _mm256_sub_epi32(_mm256_set1_epi32(1 << 15), srcAO); // as1 = 1 - srcA * o

        dstB = _mm256_add_epi32(mul_avx2(dstB, as1), mul_avx2(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm256_add_epi32(mul_avx2(dstG, as1), mul_avx2(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm256_add_epi32(mul_avx2(dstR, as1), mul_avx2(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm256_add_epi32(mul_avx2(dstA, as1), srcAO);             // dstA = (dstA * as1) + (srcA * o)

        // store
        store_avx2(dstB, dstG, dstR, dstA, &dst[i]);
    }
    // clang-format on
}
DP_TARGET_END
#pragma endregion

#pragma region seq
DP_FORCE_INLINE uint32_t mul_seq(uint32_t a, uint32_t b)
{
    return (a * b) >> 15;
}

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
    DP_CPU_SUPPORT cpu_support = DP_get_cpu_support();

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
    DP_CPU_SUPPORT cpu_support = DP_get_cpu_support();

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

#define BENCH_BLEND(name, func)                                           \
    static void name(benchmark::State &state)                             \
    {                                                                     \
        DP_Pixel15 *dst =                                                 \
            (DP_Pixel15 *)DP_malloc(DP_TILE_LENGTH * sizeof(DP_Pixel15)); \
        DP_Pixel15 *src =                                                 \
            (DP_Pixel15 *)DP_malloc(DP_TILE_LENGTH * sizeof(DP_Pixel15)); \
        memset(dst, 0, sizeof(DP_TILE_LENGTH * sizeof(DP_Pixel15)));      \
        memset(src, 0, sizeof(DP_TILE_LENGTH * sizeof(DP_Pixel15)));      \
                                                                          \
        for (auto _ : state) {                                            \
            benchmark::DoNotOptimize(dst);                                \
            benchmark::DoNotOptimize(src);                                \
                                                                          \
            no_inline_func(func, dst, src, DP_TILE_LENGTH, DP_BIT15);     \
                                                                          \
            benchmark::DoNotOptimize(dst);                                \
        }                                                                 \
                                                                          \
        DP_free(dst);                                                     \
        DP_free(src);                                                     \
    }                                                                     \
    BENCHMARK(name)

BENCH_BLEND(nop, nop_);
BENCH_BLEND(raw_sse42, blend_pixels_normal_sse42);
BENCH_BLEND(raw_avx2, blend_pixels_normal_avx2);
BENCH_BLEND(raw_seq, blend_pixels_normal_seq);
BENCH_BLEND(cpu_support_branching, blend_pixels_normal_cpu_support_branching);
BENCH_BLEND(cpu_support_ptr, blend_pixels_normal_cpu_support_ptr);
DISABLE_OPT_END

BENCHMARK_MAIN();
