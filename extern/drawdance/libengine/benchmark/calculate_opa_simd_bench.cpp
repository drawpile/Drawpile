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

static void calculate_opa_mask_seq(uint16_t *mask, float *rr_mask, int count,
                                   float hardness, float segment1_offset,
                                   float segment1_slope, float segment2_offset,
                                   float segment2_slope)
{

    for (int i = 0; i < count; ++i) {
        float rr = rr_mask[i];

        float opa = 0.0f;
        if (rr > 1.0f) {
            opa = 0.0f;
        }
        else {
            if (rr <= hardness) {
                opa = segment1_offset + rr * segment1_slope;
            }
            else {
                opa = segment2_offset + rr * segment2_slope;
            }
        }

        mask[i] = (uint16_t)(opa * (float)DP_BIT15);
    }
}

#pragma region avx
DP_TARGET_BEGIN("avx2")
DP_FORCE_INLINE __m256i load_and_calculate_avx2_blend(
    float *rr_mask, float hardness, float segment1_offset, float segment1_slope,
    float segment2_offset, float segment2_slope)
{
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    __m256 rr = _mm256_load_ps(rr_mask);

    __m256 if_gt_1 = _mm256_set1_ps(0.0f);

    __m256 if_le_hardness =
        _mm256_add_ps(_mm256_set1_ps(segment1_offset),
                      _mm256_mul_ps(rr, _mm256_set1_ps(segment1_slope)));
    __m256 else_le_hardness =
        _mm256_add_ps(_mm256_set1_ps(segment2_offset),
                      _mm256_mul_ps(rr, _mm256_set1_ps(segment2_slope)));

    __m256 if_gt_1_mask = _mm256_cmp_ps(rr, _mm256_set1_ps(1.0f), _CMP_GT_OS);
    __m256 le_hardness_mask =
        _mm256_cmp_ps(rr, _mm256_set1_ps(hardness), _CMP_LE_OS);

    __m256 opa = _mm256_blendv_ps(
        _mm256_blendv_ps(else_le_hardness, if_le_hardness, le_hardness_mask),
        if_gt_1, if_gt_1_mask);

    // Convert to 32 bit and shuffle to 16
    return _mm256_cvtps_epi32(
        _mm256_mul_ps(opa, _mm256_set1_ps((float)DP_BIT15)));
}

DP_FORCE_INLINE __m256i load_and_calculate_avx2_and(
    float *rr_mask, float hardness, float segment1_offset, float segment1_slope,
    float segment2_offset, float segment2_slope)
{
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    __m256 rr = _mm256_load_ps(rr_mask);

    __m256 if_gt_1 = _mm256_set1_ps(0.0f);

    __m256 if_le_hardness =
        _mm256_add_ps(_mm256_set1_ps(segment1_offset),
                      _mm256_mul_ps(rr, _mm256_set1_ps(segment1_slope)));
    __m256 else_le_hardness =
        _mm256_add_ps(_mm256_set1_ps(segment2_offset),
                      _mm256_mul_ps(rr, _mm256_set1_ps(segment2_slope)));

    __m256 if_gt_1_mask = _mm256_cmp_ps(rr, _mm256_set1_ps(1.0f), _CMP_GT_OS);
    __m256 le_hardness_mask =
        _mm256_cmp_ps(rr, _mm256_set1_ps(hardness), _CMP_LE_OS);

    __m256 hardness_res =
        _mm256_or_ps(_mm256_and_ps(if_le_hardness, le_hardness_mask),
                     _mm256_andnot_ps(else_le_hardness, le_hardness_mask));

    __m256 opa = _mm256_or_ps(_mm256_and_ps(if_gt_1, if_gt_1_mask),
                              _mm256_andnot_ps(hardness_res, if_gt_1_mask));

    // Convert to 32 bit and shuffle to 16
    return _mm256_cvtps_epi32(
        _mm256_mul_ps(opa, _mm256_set1_ps((float)DP_BIT15)));
}

static void calculate_opa_mask_avx2_store_128(uint16_t *mask, float *rr_mask,
                                              int count, float hardness,
                                              float segment1_offset,
                                              float segment1_slope,
                                              float segment2_offset,
                                              float segment2_slope)
{
    DP_ASSERT(count % 8 == 0);
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    for (int i = 0; i < count; i += 8) {
        __m256i _32 = load_and_calculate_avx2_blend(
            &rr_mask[i], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);
        __m256i _16 = _mm256_shuffle_epi8(
            _32, _mm256_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, -1, -1, -1, -1, -1,
                                  -1, -1, -1, 0, 1, 4, 5, 8, 9, 12, 13, -1, -1,
                                  -1, -1, -1, -1, -1, -1));
        _16 = _mm256_permute4x64_epi64(_16, 8);

        _mm_store_si128((__m128i *)&mask[i], _mm256_extracti128_si256(_16, 0));
    }
}
static void calculate_opa_mask_avx2_store_256_blend(
    uint16_t *mask, float *rr_mask, int count, float hardness,
    float segment1_offset, float segment1_slope, float segment2_offset,
    float segment2_slope)
{
    DP_ASSERT(count % 16 == 0);
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    for (int i = 0; i < count; i += 16) {
        __m256i _32_1 = load_and_calculate_avx2_blend(
            &rr_mask[i], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);
        __m256i _32_2 = load_and_calculate_avx2_blend(
            &rr_mask[i + 8], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);

        __m256i combined = _mm256_packus_epi32(_32_1, _32_2);
        __m256i _16 = _mm256_permute4x64_epi64(combined, 0xD8);

        //  __m256i _16_1 = _mm256_shuffle_epi8(
        //      _32_1, _mm256_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, -1, -1, -1,
        //      -1,
        //                              -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 12,
        //                              13, -1, -1, -1, -1, -1, -1, -1, -1));
        //  __m256i _16_2 = _mm256_shuffle_epi8(
        //      _32_2, _mm256_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4,
        //      5,
        //                              8, 9, 12, 13, -1, -1, -1, -1, -1, -1,
        //                              -1, -1, 0, 1, 4, 5, 8, 9, 12, 13));
        //  __m256i combined = _mm256_or_si256(_16_1, _16_2);
        //  __m256i _16 = _mm256_permute4x64_epi64(combined, 216);

        _mm256_store_si256((__m256i *)&mask[i], _16);
    }
}

static void calculate_opa_mask_avx2_store_256_and(
    uint16_t *mask, float *rr_mask, int count, float hardness,
    float segment1_offset, float segment1_slope, float segment2_offset,
    float segment2_slope)
{
    DP_ASSERT(count % 16 == 0);
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    for (int i = 0; i < count; i += 16) {
        __m256i _32_1 = load_and_calculate_avx2_and(
            &rr_mask[i], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);
        __m256i _32_2 = load_and_calculate_avx2_and(
            &rr_mask[i + 8], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);

        __m256i combined = _mm256_packus_epi32(_32_1, _32_2);
        __m256i _16 = _mm256_permute4x64_epi64(combined, 0xD8);

        //  __m256i _16_1 = _mm256_shuffle_epi8(
        //      _32_1, _mm256_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, -1, -1, -1,
        //      -1,
        //                              -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 12,
        //                              13, -1, -1, -1, -1, -1, -1, -1, -1));
        //  __m256i _16_2 = _mm256_shuffle_epi8(
        //      _32_2, _mm256_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4,
        //      5,
        //                              8, 9, 12, 13, -1, -1, -1, -1, -1, -1,
        //                              -1, -1, 0, 1, 4, 5, 8, 9, 12, 13));
        //  __m256i combined = _mm256_or_si256(_16_1, _16_2);
        //  __m256i _16 = _mm256_permute4x64_epi64(combined, 216);

        _mm256_store_si256((__m256i *)&mask[i], _16);
    }
}
DP_TARGET_END
#pragma endregion


DISABLE_OPT_BEGIN
#define BENCH_OPA(name, func)                                            \
    static void name(benchmark::State &state)                            \
    {                                                                    \
        uint64_t count = state.range(0);                                 \
                                                                         \
        uint16_t *mask =                                                 \
            (uint16_t *)DP_malloc_simd_zeroed(count * sizeof(uint16_t)); \
        float *rr_mask =                                                 \
            (float *)DP_malloc_simd_zeroed(count * sizeof(float));       \
                                                                         \
        float hardness = (float)rand();                                  \
        float segment1_offset = (float)rand();                           \
        float segment1_slope = (float)rand();                            \
        float segment2_offset = (float)rand();                           \
        float segment2_slope = (float)rand();                            \
                                                                         \
        for (auto _ : state) {                                           \
            benchmark::DoNotOptimize(mask);                              \
            benchmark::DoNotOptimize(rr_mask);                           \
            benchmark::DoNotOptimize(hardness);                          \
            benchmark::DoNotOptimize(segment1_offset);                   \
            benchmark::DoNotOptimize(segment1_slope);                    \
            benchmark::DoNotOptimize(segment2_offset);                   \
            benchmark::DoNotOptimize(segment2_slope);                    \
                                                                         \
            func(mask, rr_mask, count, hardness, segment1_offset,        \
                 segment1_slope, segment2_offset, segment2_slope);       \
                                                                         \
            benchmark::DoNotOptimize(mask);                              \
        }                                                                \
                                                                         \
        DP_free_simd(mask);                                              \
        DP_free_simd(rr_mask);                                           \
    }                                                                    \
    BENCHMARK(name)->Args({64})->Args({224})->Args({1504})

BENCH_OPA(seq, calculate_opa_mask_seq);
BENCH_OPA(avx2_store_128, calculate_opa_mask_avx2_store_128);
BENCH_OPA(avx2_store_256_blend, calculate_opa_mask_avx2_store_256_blend);
BENCH_OPA(avx2_store_256_and, calculate_opa_mask_avx2_store_256_and);
DISABLE_OPT_END

BENCHMARK_MAIN();
