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

#pragma region seq
static void blend_line_seq(DP_Pixel15 *dst, DP_UPixel15 src,
                           const uint16_t *mask, uint32_t opacity, int count)
{
    for (int x = 0; x < count; ++x, ++dst, ++mask) {
        uint32_t o = mul_seq(*mask, opacity);

        uint32_t opa_a = mul_seq(o, src.a);
        uint32_t opa_b = DP_BIT15 - o;

        dst->b = sumprods_seq(opa_a, src.b, opa_b, dst->b);
        dst->g = sumprods_seq(opa_a, src.g, opa_b, dst->g);
        dst->r = sumprods_seq(opa_a, src.r, opa_b, dst->r);
        dst->a = opa_a + ((opa_b * dst->a) / DP_BIT15);
    }
}


static void blend_mask_normal_and_eraser_seq(DP_Pixel15 *dst, DP_UPixel15 src,
                                             const uint16_t *mask,
                                             uint32_t opacity, int w, int h,
                                             int mask_skip, int base_skip)
{
    for (int y = 0; y < h; ++y) {
        blend_line_seq(dst, src, mask, opacity, w);
        dst += w;
        mask += w;

        dst += base_skip;
        mask += mask_skip;
    }
}
#pragma endregion

#pragma region sse
DP_TARGET_BEGIN("sse4.2")
static void blend_line_sse42(DP_Pixel15 *dst, DP_UPixel15 src,
                             const uint16_t *mask_int, uint32_t opacity_int,
                             int count)
{
    DP_ASSERT(count % 4 == 0);

    // Do in parent function ?
    __m128i srcB = _mm_set1_epi32(src.b);
    __m128i srcG = _mm_set1_epi32(src.g);
    __m128i srcR = _mm_set1_epi32(src.r);
    __m128i srcA = _mm_set1_epi32(src.a);

    __m128i opacity = _mm_set1_epi32(opacity_int);
    __m128i bit15 = _mm_set1_epi32(DP_BIT15);

    for (int x = 0; x < count; x += 4, dst += 4, mask_int += 4) {
        // load mask
        __m128i mask = _mm_cvtepu16_epi32(_mm_loadl_epi64((__m128i *)mask_int));

        // Load dest
        __m128i dstB, dstG, dstR, dstA;
        load_unaligned_sse42(dst, &dstB, &dstG, &dstR, &dstA);

        __m128i o = mul_sse42(mask, opacity);
        __m128i opa_a = mul_sse42(o, srcA);
        __m128i opa_b = _mm_sub_epi32(bit15, o);

        dstB = sumprods_sse42(opa_a, srcB, opa_b, dstB);
        dstG = sumprods_sse42(opa_a, srcG, opa_b, dstG);
        dstR = sumprods_sse42(opa_a, srcR, opa_b, dstR);
        dstA = _mm_add_epi32(opa_a,
                             _mm_srli_epi32(_mm_mullo_epi32(opa_b, dstA), 15));

        store_unaligned_sse42(dstB, dstG, dstR, dstA, dst);
    }
}

static void blend_mask_normal_and_eraser_sse42(DP_Pixel15 *dst, DP_UPixel15 src,
                                               const uint16_t *mask,
                                               uint32_t opacity, int w, int h,
                                               int mask_skip, int base_skip)
{

    int remaining_width = w % 4;
    int sse_width = w - remaining_width;

    for (int y = 0; y < h; ++y) {
        blend_line_sse42(dst, src, mask, opacity, sse_width);
        dst += sse_width;
        mask += sse_width;

        blend_line_seq(dst, src, mask, opacity, remaining_width);
        dst += remaining_width;
        mask += remaining_width;

        dst += base_skip;
        mask += mask_skip;
    }
}
DP_TARGET_END
#pragma endregion

#pragma region avx
DP_TARGET_BEGIN("avx2")
static void blend_line_avx2(DP_Pixel15 *dst, DP_UPixel15 src,
                            const uint16_t *mask_int, uint32_t opacity_int,
                            int count)
{
    DP_ASSERT(count % 8 == 0);

    // Do in parent function ?
    __m256i srcB = _mm256_set1_epi32(src.b);
    __m256i srcG = _mm256_set1_epi32(src.g);
    __m256i srcR = _mm256_set1_epi32(src.r);
    __m256i srcA = _mm256_set1_epi32(src.a);

    __m256i opacity = _mm256_set1_epi32(opacity_int);
    __m256i bit15 = _mm256_set1_epi32(DP_BIT15);

    for (int x = 0; x < count; x += 8, dst += 8, mask_int += 8) {
        // load mask
        __m256i mask =
            _mm256_cvtepu16_epi32(_mm_load_si128((__m128i *)mask_int));
        // Permute mask to fit pixel load order (15263748)
        mask = _mm256_permutevar8x32_epi32(
            mask, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));

        // Load dst
        __m256i dstB, dstG, dstR, dstA;
        load_unaligned_avx2(dst, &dstB, &dstG, &dstR, &dstA);

        __m256i o = mul_avx2(mask, opacity);
        __m256i opa_a = mul_avx2(o, srcA);
        __m256i opa_b = _mm256_sub_epi32(bit15, o);

        dstB = sumprods_avx2(opa_a, srcB, opa_b, dstB);
        dstG = sumprods_avx2(opa_a, srcG, opa_b, dstG);
        dstR = sumprods_avx2(opa_a, srcR, opa_b, dstR);
        dstA = _mm256_add_epi32(
            opa_a, _mm256_srli_epi32(_mm256_mullo_epi32(opa_b, dstA), 15));

        store_unaligned_avx2(dstB, dstG, dstR, dstA, dst);
    }
    _mm256_zeroupper();
}

static void blend_line_avx2_aligned(DP_Pixel15 *dst, DP_UPixel15 src,
                                    const uint16_t *mask_int,
                                    uint32_t opacity_int, int count)
{
    DP_ASSERT(count % 8 == 0);

    // Do in parent function ?
    __m256i srcB = _mm256_set1_epi32(src.b);
    __m256i srcG = _mm256_set1_epi32(src.g);
    __m256i srcR = _mm256_set1_epi32(src.r);
    __m256i srcA = _mm256_set1_epi32(src.a);

    __m256i opacity = _mm256_set1_epi32(opacity_int);
    __m256i bit15 = _mm256_set1_epi32(DP_BIT15);

    for (int x = 0; x < count; x += 8, dst += 8, mask_int += 8) {
        // load mask
        __m256i mask =
            _mm256_cvtepu16_epi32(_mm_load_si128((__m128i *)mask_int));
        // Permute mask to fit pixel load order (15263748)
        mask = _mm256_permutevar8x32_epi32(
            mask, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));

        // Load dst
        __m256i dstB, dstG, dstR, dstA;
        load_unaligned_avx2(dst, &dstB, &dstG, &dstR, &dstA);

        __m256i o = mul_avx2(mask, opacity);
        __m256i opa_a = mul_avx2(o, srcA);
        __m256i opa_b = _mm256_sub_epi32(bit15, o);

        dstB = sumprods_avx2(opa_a, srcB, opa_b, dstB);
        dstG = sumprods_avx2(opa_a, srcG, opa_b, dstG);
        dstR = sumprods_avx2(opa_a, srcR, opa_b, dstR);
        dstA = _mm256_add_epi32(
            opa_a, _mm256_srli_epi32(_mm256_mullo_epi32(opa_b, dstA), 15));

        store_aligned_avx2(dstB, dstG, dstR, dstA, dst);
    }
    _mm256_zeroupper();
}

static void blend_mask_normal_and_eraser_avx2(DP_Pixel15 *dst, DP_UPixel15 src,
                                              const uint16_t *mask,
                                              uint32_t opacity, int w, int h,
                                              int mask_skip, int base_skip)
{
    int remaining_after_avx_width = w % 8;
    int avx_width = w - remaining_after_avx_width;

    int remaining_after_sse_width = remaining_after_avx_width % 4;
    int sse_width = remaining_after_avx_width - remaining_after_sse_width;

    for (int y = 0; y < h; ++y) {
        blend_line_avx2(dst, src, mask, opacity, avx_width);
        dst += avx_width;
        mask += avx_width;

        blend_line_sse42(dst, src, mask, opacity, sse_width);
        dst += sse_width;
        mask += sse_width;

        blend_line_seq(dst, src, mask, opacity, remaining_after_sse_width);
        dst += remaining_after_sse_width;
        mask += remaining_after_sse_width;

        dst += base_skip;
        mask += mask_skip;
    }
}

static void blend_mask_normal_and_eraser_avx2_aligned(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, uint32_t opacity,
    int w, int h, int mask_skip, int base_skip)
{
    int modulo = (uintptr_t)dst % 32;
    int to_alignement = modulo == 0 ? 0 : (32 - modulo) / sizeof(DP_Pixel15);
    to_alignement = DP_min_int(to_alignement, w);
    int remaining_after_alignement = w - to_alignement;

    int remaining_after_avx_width = remaining_after_alignement % 8;
    int avx_width = remaining_after_alignement - remaining_after_avx_width;

    int remaining_after_sse_width = remaining_after_avx_width % 4;
    int sse_width = remaining_after_avx_width - remaining_after_sse_width;

    for (int y = 0; y < h; ++y) {
        blend_line_seq(dst, src, mask, opacity, to_alignement);
        dst += to_alignement;
        mask += to_alignement;

        blend_line_avx2_aligned(dst, src, mask, opacity, avx_width);
        dst += avx_width;
        mask += avx_width;


        blend_line_sse42(dst, src, mask, opacity, sse_width);
        dst += sse_width;
        mask += sse_width;

        blend_line_seq(dst, src, mask, opacity, remaining_after_sse_width);
        dst += remaining_after_sse_width;
        mask += remaining_after_sse_width;

        dst += base_skip;
        mask += mask_skip;
    }
}
DP_TARGET_END
#pragma endregion

static void blend_mask_normal_and_eraser_cpu_branch(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, uint32_t opacity,
    int w, int h, int mask_skip, int base_skip)
{
    for (int y = 0; y < h; ++y) {
        int remaining = w;

        if (DP_cpu_support >= DP_CPU_SUPPORT_AVX2) {
            int remaining_after_avx_width = remaining % 8;
            int avx_width = remaining - remaining_after_avx_width;

            blend_line_avx2(dst, src, mask, opacity, avx_width);

            remaining -= avx_width;
            dst += avx_width;
            mask += avx_width;
        }

        if (DP_cpu_support >= DP_CPU_SUPPORT_SSE42) {
            int remaining_after_sse_width = remaining % 4;
            int sse_width = remaining - remaining_after_sse_width;

            blend_line_sse42(dst, src, mask, opacity, sse_width);

            remaining -= sse_width;
            dst += sse_width;
            mask += sse_width;
        }

        blend_line_seq(dst, src, mask, opacity, remaining);
        dst += remaining;
        mask += remaining;

        dst += base_skip;
        mask += mask_skip;
    }
}

static void nop_(DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask,
                 uint32_t opacity, int w, int h, int mask_skip, int base_skip)
{
}


#define BENCH_BLEND(name, func)                                               \
    static void name(benchmark::State &state)                                 \
    {                                                                         \
        DP_Pixel15 *dst = (DP_Pixel15 *)DP_malloc_simd_zeroed(                \
            DP_TILE_LENGTH * sizeof(DP_Pixel15));                             \
                                                                              \
                                                                              \
        DP_UPixel15 src = {0};                                                \
                                                                              \
        uint16_t *mask = (uint16_t *)DP_malloc_simd_zeroed(                   \
            DP_TILE_LENGTH * sizeof(uint16_t));                               \
                                                                              \
                                                                              \
        int w = state.range(0);                                               \
        int h = 64;                                                           \
        int skip = 64 - w;                                                    \
                                                                              \
        for (auto _ : state) {                                                \
            benchmark::DoNotOptimize(dst);                                    \
            benchmark::DoNotOptimize(src);                                    \
            benchmark::DoNotOptimize(mask);                                   \
                                                                              \
            no_inline_func(func, dst, src, mask, DP_BIT15, w, h, skip, skip); \
                                                                              \
            benchmark::DoNotOptimize(dst);                                    \
        }                                                                     \
                                                                              \
        DP_free_simd(dst);                                                    \
        DP_free_simd(mask);                                                   \
    }                                                                         \
    BENCHMARK(name)                                                           \
        ->Args({2})                                                           \
        ->Args({4})                                                           \
        ->Args({16})                                                          \
        ->Args({32})                                                          \
        ->Args({35})                                                          \
        ->Args({47})                                                          \
        ->Args({53})                                                          \
        ->Args({64})

// BENCH_BLEND(nop, nop_);
BENCH_BLEND(seq, blend_mask_normal_and_eraser_seq);
BENCH_BLEND(sse42, blend_mask_normal_and_eraser_sse42);
BENCH_BLEND(avx2, blend_mask_normal_and_eraser_avx2);
BENCH_BLEND(avx2_aligned, blend_mask_normal_and_eraser_avx2_aligned);
BENCH_BLEND(branch, blend_mask_normal_and_eraser_cpu_branch);

BENCHMARK_MAIN();

// Validate that the SSE and AVX implementations return the same thing as the
// sequential implementation.
DP_UNUSED int validate()
{

    // Init
    DP_Pixel15 *dst = (DP_Pixel15 *)DP_malloc_simd_zeroed(DP_TILE_LENGTH
                                                          * sizeof(DP_Pixel15));
    for (int i = 0; i < DP_TILE_LENGTH; i++) {
        dst[i].b = rand() % DP_BIT15;
        dst[i].g = rand() % DP_BIT15;
        dst[i].r = rand() % DP_BIT15;
        dst[i].a = rand() % DP_BIT15;
    }
    DP_UPixel15 src = {
        (uint16_t)(rand() % DP_BIT15), (uint16_t)(rand() % DP_BIT15),
        (uint16_t)(rand() % DP_BIT15), (uint16_t)(rand() % DP_BIT15)};

    uint16_t *mask =
        (uint16_t *)DP_malloc_simd_zeroed(DP_TILE_LENGTH * sizeof(uint16_t));

    for (int i = 0; i < DP_TILE_LENGTH; i++) {
        mask[i] = rand() % DP_BIT15;
    }

    uint32_t opacity = rand() % DP_BIT15;

    // Seq
    DP_Pixel15 *dst_seq = (DP_Pixel15 *)DP_malloc_simd_zeroed(
        DP_TILE_LENGTH * sizeof(DP_Pixel15));
    memcpy(dst_seq, dst, DP_TILE_LENGTH * sizeof(DP_Pixel15));

    blend_mask_normal_and_eraser_seq(dst_seq, src, mask, opacity, 64, 35,
                                     64 - 35, 64 - 35);

    // SSE
    DP_Pixel15 *dst_sse = (DP_Pixel15 *)DP_malloc_simd_zeroed(
        DP_TILE_LENGTH * sizeof(DP_Pixel15));
    memcpy(dst_sse, dst, DP_TILE_LENGTH * sizeof(DP_Pixel15));

    blend_mask_normal_and_eraser_sse42(dst_sse, src, mask, opacity, 64, 35,
                                       64 - 35, 64 - 35);

    // AVX
    DP_Pixel15 *dst_avx = (DP_Pixel15 *)DP_malloc_simd_zeroed(
        DP_TILE_LENGTH * sizeof(DP_Pixel15));
    memcpy(dst_avx, dst, DP_TILE_LENGTH * sizeof(DP_Pixel15));

    blend_mask_normal_and_eraser_avx2_aligned(dst_avx, src, mask, opacity, 64,
                                              35, 64 - 35, 64 - 35);

    for (int i = 0; i < DP_TILE_LENGTH; i++) {
        // clang-format off
        if(dst_seq[i].b != dst_sse[i].b) DP_panic("No match at index %d : SEQ %d SSE %d", i, dst_seq[i].b, dst_sse[i].b);
        if(dst_seq[i].g != dst_sse[i].g) DP_panic("No match at index %d : SEQ %d SSE %d", i, dst_seq[i].g, dst_sse[i].g);
        if(dst_seq[i].r != dst_sse[i].r) DP_panic("No match at index %d : SEQ %d SSE %d", i, dst_seq[i].r, dst_sse[i].r);
        if(dst_seq[i].a != dst_sse[i].a) DP_panic("No match at index %d : SEQ %d SSE %d", i, dst_seq[i].a, dst_sse[i].a);

        if(dst_seq[i].b != dst_avx[i].b) DP_panic("No match at index %d : SEQ %d AVX %d", i, dst_seq[i].b, dst_avx[i].b);
        if(dst_seq[i].g != dst_avx[i].g) DP_panic("No match at index %d : SEQ %d AVX %d", i, dst_seq[i].g, dst_avx[i].g);
        if(dst_seq[i].r != dst_avx[i].r) DP_panic("No match at index %d : SEQ %d AVX %d", i, dst_seq[i].r, dst_avx[i].r);
        if(dst_seq[i].a != dst_avx[i].a) DP_panic("No match at index %d : SEQ %d AVX %d", i, dst_seq[i].a, dst_avx[i].a);
        // clang-format on
    }

    return 0;
}
