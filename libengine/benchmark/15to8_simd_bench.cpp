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

void sse42_unpack(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{

    assert(count % 4 == 0);

    for (int i = 0; i < count; i += 4) {
        __m128i source1 = _mm_load_si128((__m128i *)&src[i]);
        __m128i source2 = _mm_load_si128((__m128i *)&src[i + 2]);

        __m128i p1 = _mm_cvtepu16_epi32(source1);
        __m128i p2 = _mm_cvtepu16_epi32(_mm_srli_si128(source1, 8));
        __m128i p3 = _mm_cvtepu16_epi32(source2);
        __m128i p4 = _mm_cvtepu16_epi32(_mm_srli_si128(source2, 8));

#define _15to8(p) p = _mm_srli_epi32(_mm_sub_epi32(_mm_slli_epi32(p, 8), p), 15)
        _15to8(p1);
        _15to8(p2);
        _15to8(p3);
        _15to8(p4);
#undef _15to8

        __m128i in_front_shuffle =
            _mm_setr_epi8(0, 4, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
                          0x80, 0x80, 0x80, 0x80, 0x80);
        p1 = _mm_shuffle_epi8(p1, in_front_shuffle);
        p2 = _mm_shuffle_epi8(p2, in_front_shuffle);
        p3 = _mm_shuffle_epi8(p3, in_front_shuffle);
        p4 = _mm_shuffle_epi8(p4, in_front_shuffle);

        __m128i p1p2 = _mm_unpacklo_epi32(p1, p2);
        __m128i p3p4 = _mm_unpacklo_epi32(p3, p4);

        __m128i p1p2p3p4 = _mm_unpacklo_epi64(p1p2, p3p4);

        _mm_store_si128((__m128i *)&dst[i], p1p2p3p4);
    }
}

void sse42_and(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{
    assert(count % 4 == 0);

    for (int i = 0; i < count; i += 4) {
        __m128i source1 = _mm_load_si128((__m128i *)&src[i]);
        __m128i source2 = _mm_load_si128((__m128i *)&src[i + 2]);

        __m128i p1 = _mm_cvtepu16_epi32(source1);
        __m128i p2 = _mm_cvtepu16_epi32(_mm_srli_si128(source1, 8));
        __m128i p3 = _mm_cvtepu16_epi32(source2);
        __m128i p4 = _mm_cvtepu16_epi32(_mm_srli_si128(source2, 8));

#define _15to8(p) p = _mm_srli_epi32(_mm_sub_epi32(_mm_slli_epi32(p, 8), p), 15)
        _15to8(p1);
        _15to8(p2);
        _15to8(p3);
        _15to8(p4);
#undef _15to8

        p2 = _mm_slli_si128(p2, 1);
        p3 = _mm_slli_si128(p3, 2);
        p4 = _mm_slli_si128(p4, 3);

        __m128i combined =
            _mm_or_si128(p1, _mm_or_si128(p2, _mm_or_si128(p3, p4)));

        __m128i out = _mm_shuffle_epi8(
            combined, _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3,
                                    7, 11, 15));

        _mm_store_si128((__m128i *)&dst[i], out);
    }
}

void sse42_and_mul(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{
    assert(count % 4 == 0);

    for (int i = 0; i < count; i += 4) {
        __m128i source1 = _mm_load_si128((__m128i *)&src[i]);
        __m128i source2 = _mm_load_si128((__m128i *)&src[i + 2]);

        __m128i p1 = _mm_cvtepu16_epi32(source1);
        __m128i p2 = _mm_cvtepu16_epi32(_mm_srli_si128(source1, 8));
        __m128i p3 = _mm_cvtepu16_epi32(source2);
        __m128i p4 = _mm_cvtepu16_epi32(_mm_srli_si128(source2, 8));

#define _15to8(p) \
    p = _mm_srli_epi32(_mm_mullo_epi32(p, _mm_set1_epi32(255)), 15)
        _15to8(p1);
        _15to8(p2);
        _15to8(p3);
        _15to8(p4);
#undef _15to8

        p2 = _mm_slli_si128(p2, 1);
        p3 = _mm_slli_si128(p3, 2);
        p4 = _mm_slli_si128(p4, 3);

        __m128i combined =
            _mm_or_si128(p1, _mm_or_si128(p2, _mm_or_si128(p3, p4)));

        __m128i out = _mm_shuffle_epi8(
            combined, _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3,
                                    7, 11, 15));

        _mm_store_si128((__m128i *)&dst[i], out);
    }
}

DP_TARGET_END
#pragma endregion

#pragma region avx
DP_TARGET_BEGIN("avx2")

void avx2_and_mul_shuffle_or_permute(DP_Pixel8 *dst, const DP_Pixel15 *src,
                                     int count)
{
    assert(count % 8 == 0);

    for (int i = 0; i < count; i += 8) {
        __m256i source1 = _mm256_loadu_si256((__m256i *)&src[i]);
        __m256i source2 = _mm256_loadu_si256((__m256i *)&src[i + 4]);

        __m256i p1 = _mm256_and_si256(source1, _mm256_set1_epi32(0xFFFF));
        __m256i p2 = _mm256_srli_epi32(
            _mm256_and_si256(source1, _mm256_set1_epi32(0xFFFF0000)), 16);
        __m256i p3 = _mm256_and_si256(source2, _mm256_set1_epi32(0xFFFF));
        __m256i p4 = _mm256_srli_epi32(
            _mm256_and_si256(source2, _mm256_set1_epi32(0xFFFF0000)), 16);


#define _15to8(p) \
    p = _mm256_srli_epi32(_mm256_mullo_epi32(p, _mm256_set1_epi32(255)), 15)
        _15to8(p1);
        _15to8(p2);
        _15to8(p3);
        _15to8(p4);
#undef _15to8

        p1 = _mm256_shuffle_epi8(
            p1, _mm256_setr_epi8(0, 0x80, 4, 0x80, 0x80, 0x80, 0x80, 0x80, 8,
                                 0x80, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0,
                                 0x80, 4, 0x80, 0x80, 0x80, 0x80, 0x80, 8, 0x80,
                                 12, 0x80, 0x80, 0x80, 0x80, 0x80));
        p2 = _mm256_shuffle_epi8(
            p2, _mm256_setr_epi8(0x80, 0, 0x80, 4, 0x80, 0x80, 0x80, 0x80, 0x80,
                                 8, 0x80, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0,
                                 0x80, 4, 0x80, 0x80, 0x80, 0x80, 0x80, 8, 0x80,
                                 12, 0x80, 0x80, 0x80, 0x80));
        p3 = _mm256_shuffle_epi8(
            p3, _mm256_setr_epi8(0x80, 0x80, 0x80, 0x80, 0, 0x80, 4, 0x80, 0x80,
                                 0x80, 0x80, 0x80, 8, 0x80, 12, 0x80, 0x80,
                                 0x80, 0x80, 0x80, 0, 0x80, 4, 0x80, 0x80, 0x80,
                                 0x80, 0x80, 8, 0x80, 12, 0x80));
        p4 = _mm256_shuffle_epi8(
            p4, _mm256_setr_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 4, 0x80,
                                 0x80, 0x80, 0x80, 0x80, 8, 0x80, 12, 0x80,
                                 0x80, 0x80, 0x80, 0x80, 0, 0x80, 4, 0x80, 0x80,
                                 0x80, 0x80, 0x80, 8, 0x80, 12));

        __m256i combined =
            _mm256_or_si256(p1, _mm256_or_si256(p2, _mm256_or_si256(p3, p4)));

        __m256i out = _mm256_permutevar8x32_epi32(
            combined, _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));

        _mm256_storeu_si256((__m256i *)&dst[i], out);
    }
}


void avx2_unpack_mul(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{
    assert(count % 8 == 0);

    for (int i = 0; i < count; i += 8) {
        __m256i source1 = _mm256_loadu_si256((__m256i *)&src[i]);
        __m256i source2 = _mm256_loadu_si256((__m256i *)&src[i + 4]);

        __m256i p1 = _mm256_unpacklo_epi16(source1, _mm256_set1_epi8(0));
        __m256i p2 = _mm256_unpackhi_epi16(source1, _mm256_set1_epi8(0));
        __m256i p3 = _mm256_unpacklo_epi16(source2, _mm256_set1_epi8(0));
        __m256i p4 = _mm256_unpackhi_epi16(source2, _mm256_set1_epi8(0));


#define _15to8(p) \
    p = _mm256_srli_epi32(_mm256_mullo_epi32(p, _mm256_set1_epi32(255)), 15)
        _15to8(p1);
        _15to8(p2);
        _15to8(p3);
        _15to8(p4);
#undef _15to8

        __m256i in_front_shuffle = _mm256_setr_epi8(
            0, 4, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0, 4, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
        p1 = _mm256_shuffle_epi8(p1, in_front_shuffle);
        p2 = _mm256_shuffle_epi8(p2, in_front_shuffle);
        p3 = _mm256_shuffle_epi8(p3, in_front_shuffle);
        p4 = _mm256_shuffle_epi8(p4, in_front_shuffle);

        __m256i p1p2 =
            _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(p1, p2), 88);
        __m256i p3p4 =
            _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(p3, p4), 88);

        __m256i p1p2p3p4 = _mm256_permute2x128_si256(p1p2, p3p4, 32);

        _mm256_storeu_si256((__m256i *)&dst[i], p1p2p3p4);
    }
}

DP_TARGET_END
#pragma endregion

void seq_int(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{
    for (int i = 0; i < count; i++) {
        dst[i].b = ((src[i].b << 8) - src[i].b) >> 15;
        dst[i].g = ((src[i].g << 8) - src[i].g) >> 15;
        dst[i].r = ((src[i].r << 8) - src[i].r) >> 15;
        dst[i].a = ((src[i].a << 8) - src[i].a) >> 15;
    }
}

void seq_float(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{
    for (int i = 0; i < count; i++) {
        dst[i].b = (float)src[i].b / (float)(1 << 15) * 255.0;
        dst[i].g = (float)src[i].g / (float)(1 << 15) * 255.0;
        dst[i].r = (float)src[i].r / (float)(1 << 15) * 255.0;
        dst[i].a = (float)src[i].a / (float)(1 << 15) * 255.0;
    }
}

DISABLE_OPT_BEGIN
#define BENCH_15TO8(name, func)                                           \
    static void name(benchmark::State &state)                             \
    {                                                                     \
        DP_Pixel15 *src =                                                 \
            (DP_Pixel15 *)DP_malloc(DP_TILE_LENGTH * sizeof(DP_Pixel15)); \
        DP_Pixel8 *dst =                                                  \
            (DP_Pixel8 *)DP_malloc(DP_TILE_LENGTH * sizeof(DP_Pixel8));   \
        memset(src, 0, sizeof(DP_TILE_LENGTH * sizeof(DP_Pixel15)));      \
        memset(dst, 0, sizeof(DP_TILE_LENGTH * sizeof(DP_Pixel8)));       \
                                                                          \
        for (auto _ : state) {                                            \
            benchmark::DoNotOptimize(dst);                                \
            benchmark::DoNotOptimize(src);                                \
                                                                          \
            func(dst, src, DP_TILE_LENGTH);                               \
                                                                          \
            benchmark::DoNotOptimize(dst);                                \
        }                                                                 \
                                                                          \
        DP_free(dst);                                                     \
        DP_free(src);                                                     \
    }                                                                     \
    BENCHMARK(name)

BENCH_15TO8(seq_int, seq_int);
BENCH_15TO8(seq_float, seq_float);
BENCH_15TO8(sse42_unpack, sse42_unpack);
BENCH_15TO8(sse42_and, sse42_and);
BENCH_15TO8(sse42_and_mul, sse42_and_mul);
BENCH_15TO8(avx2_unpack_mul, avx2_unpack_mul);
BENCH_15TO8(avx2_and_mul_shuffle_or_permute, avx2_and_mul_shuffle_or_permute);
DISABLE_OPT_END
BENCHMARK_MAIN();

/* int main()
{
    DP_Pixel15 src[] = {{1, 2, 3, 4},         {10, 20, 30, 40},
                        {100, 200, 300, 400}, {1000, 2000, 3000, 4000},
                        {5, 6, 7, 8},         {50, 60, 70, 80},
                        {500, 600, 700, 800}, {5000, 6000, 7000, 8000}};
    DP_Pixel8 dst[DP_ARRAY_LENGTH(src)] = {0};

    avx2_and_mul_shuffle_or_permute(dst, src, DP_ARRAY_LENGTH(src));
    __debugbreak();
    return 0;
} */
