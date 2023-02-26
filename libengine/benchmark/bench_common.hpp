#pragma once

#include <emmintrin.h>
extern "C" {
#include "dpcommon/common.h"
#include "dpcommon/cpu.h"
#include "dpengine/pixels.h"
}

struct RunBeforeMain {
    inline RunBeforeMain()
    {
        DP_cpu_support_init();
    }
};
static RunBeforeMain run_before_main;

#if defined(__clang__)
#    define DISABLE_OPT_BEGIN _Pragma("clang optimize off")
#    define DISABLE_OPT_END   _Pragma("clang optimize on")
#elif defined(__GNUC__)
#    define DISABLE_OPT_BEGIN \
        _Pragma("GCC push_options") _Pragma("GCC optimize(\"O0\")")
#    define DISABLE_OPT_END _Pragma("GCC pop_options")
#elif defined(_MSC_VER)
#    define DISABLE_OPT_BEGIN _Pragma("optimize(\"\", off)")
#    define DISABLE_OPT_END   _Pragma("optimize(\"\", on)")
#endif

DISABLE_OPT_BEGIN
template <class F, class... Args>
static void DP_NOINLINE no_inline_func(F func, Args... args)
{
    func(args...);
}
DISABLE_OPT_END

DP_TARGET_BEGIN("sse4.2")
#pragma region load
DP_INLINE void shuffle_load_sse42(__m128i source1, __m128i source2,
                                  __m128i *out_blue, __m128i *out_green,
                                  __m128i *out_red, __m128i *out_alpha)
{
    // clang-format off
    // source1 |B1|G1|R1|A1|B2|G2|R2|A2|
    // source2 |B3|G3|R3|A3|B4|G4|R4|A4|

    __m128i shuffled1 = _mm_shuffle_epi32(source1, _MM_SHUFFLE(3, 1, 2, 0)); // |B1|G1|B2|G2|R1|A1|R2|A2|
    __m128i shuffled2 = _mm_shuffle_epi32(source2, _MM_SHUFFLE(3, 1, 2, 0)); // |B3|G3|B4|G4|R3|A3|R3|A3|

    __m128i blue_green = _mm_unpacklo_epi64(shuffled1, shuffled2); // |B1|G1|B2|G2|B3|G3|B4|G4|
    __m128i red_alpha = _mm_unpackhi_epi64(shuffled1, shuffled2);  // |R1|A1|R2|A2|R3|A3|R4|A4|

    *out_blue = _mm_blend_epi16(blue_green, _mm_set1_epi32(0), 170); // Zero out upper bits (16 -> 32)
    *out_green = _mm_srli_epi32(blue_green, 16);                     // Move and zero out upper bits (16 -> 32)
    *out_red = _mm_blend_epi16(red_alpha, _mm_set1_epi32(0), 170);   // Zero out upper bits (16 -> 32)
    *out_alpha = _mm_srli_epi32(red_alpha, 16);                      // Move and zero out upper bits (16 -> 32)
    // clang-format on
}

// Load 4 16bit pixels and split them into 4x32 bit registers.
DP_INLINE void load_aligned_sse42(const DP_Pixel15 src[4], __m128i *out_blue,
                                  __m128i *out_green, __m128i *out_red,
                                  __m128i *out_alpha)
{
    DP_ASSERT(((intptr_t)src) % 16 == 0);

    __m128i source1 = _mm_load_si128((const __m128i *)src);
    __m128i source2 = _mm_load_si128((const __m128i *)src + 1);

    shuffle_load_sse42(source1, source2, out_blue, out_green, out_red,
                       out_alpha);
}

DP_INLINE void load_unaligned_sse42(const DP_Pixel15 src[4], __m128i *out_blue,
                                    __m128i *out_green, __m128i *out_red,
                                    __m128i *out_alpha)
{
    __m128i source1 = _mm_loadu_si128((const __m128i *)src);
    __m128i source2 = _mm_loadu_si128((const __m128i *)src + 1);

    shuffle_load_sse42(source1, source2, out_blue, out_green, out_red,
                       out_alpha);
}
#pragma endregion

#pragma region store
DP_INLINE void shuffle_store_sse42(__m128i blue, __m128i green, __m128i red,
                                   __m128i alpha, __m128i *out1, __m128i *out2)
{
    // clang-format off
    __m128i blue_green = _mm_blend_epi16(blue, _mm_slli_si128(green, 2), 170); // |B1|G1|B2|G2|B3|G3|B4|G4|
    __m128i red_alpha = _mm_blend_epi16(red, _mm_slli_si128(alpha, 2), 170);   // |R1|A1|R2|A2|R3|A3|R4|A4|

    *out1 = _mm_unpacklo_epi32(blue_green, red_alpha); // |B1|G1|R1|A1|B2|G2|R2|A2|
    *out2 = _mm_unpackhi_epi32(blue_green, red_alpha); // |B3|G3|R3|A3|B4|G4|R4|A4|
    // clang-format on
}

// Store 4x32 bit registers into 4 16bit pixels.
DP_INLINE void store_aligned_sse42(__m128i blue, __m128i green, __m128i red,
                                   __m128i alpha, DP_Pixel15 dest[4])
{

    DP_ASSERT(((intptr_t)dest) % 16 == 0);

    __m128i out1, out2;
    shuffle_store_sse42(blue, green, red, alpha, &out1, &out2);

    _mm_store_si128((__m128i *)dest, out1);
    _mm_store_si128((__m128i *)dest + 1, out2);
}

// Store 4x32 bit registers into 4 16bit pixels.
DP_INLINE void store_unaligned_sse42(__m128i blue, __m128i green, __m128i red,
                                     __m128i alpha, DP_Pixel15 dest[4])
{
    __m128i out1, out2;
    shuffle_store_sse42(blue, green, red, alpha, &out1, &out2);

    _mm_storeu_si128((__m128i *)dest, out1);
    _mm_storeu_si128((__m128i *)dest + 1, out2);
}
#pragma endregion

DP_INLINE __m128i mul_sse42(__m128i a, __m128i b)
{
    return _mm_srli_epi32(_mm_mullo_epi32(a, b), 15);
}

DP_INLINE __m128i sumprods_sse42(__m128i a1, __m128i a2, __m128i b1, __m128i b2)
{
    return _mm_srli_epi32(
        _mm_add_epi32(_mm_mullo_epi32(a1, a2), _mm_mullo_epi32(b1, b2)), 15);
}
DP_TARGET_END

DP_TARGET_BEGIN("avx2")

#pragma region load
DP_INLINE void shuffle_load_avx2(__m256i source1, __m256i source2,
                                 __m256i *out_blue, __m256i *out_green,
                                 __m256i *out_red, __m256i *out_alpha)
{
    // clang-format off

    // source1 |B1|G1|R1|A1|B2|G2|R2|A2|B3|G3|R3|A3|B4|G4|R4|A4|
    // source2 |B5|G5|R5|A5|B6|G6|R6|A6|B7|G7|R7|A7|B8|G8|R8|A8|

    __m256i lo = _mm256_unpacklo_epi32(source1, source2);     // |B1|G1|B5|G5|R1|A1|R5|A5|B3|G3|B7|G7|R3|A3|R7|A7|
    __m256i hi = _mm256_unpackhi_epi32(source1, source2);     // |B2|G2|B6|G6|R2|A2|R6|A6|B4|G4|B8|G8|R4|A4|R8|A8|

    __m256i blue_green = _mm256_unpacklo_epi64(lo, hi);       // |B1|G1|B5|G5|B2|G2|B6|G6|B3|G3|B7|G7|B4|G4|B8|G8|
    __m256i red_alpha = _mm256_unpackhi_epi64(lo, hi);        // |R1|A1|R5|A5|R2|A2|R6|A6|R3|A3|R7|A7|R4|A4|R8|A8|

    // Blend with 0 to zero upper bits of 32 spot. Shifting conveniently also zeros out upper bits.
    *out_blue = _mm256_blend_epi16(blue_green, _mm256_set1_epi32(0), 170); // |B1|0|B5|0|B2|0|B6|0|B3|0|B7|0|B4|0|B8|0|
    *out_green = _mm256_srli_epi32(blue_green, 16);                        // |G1|0|G5|0|G2|0|G6|0|G3|0|G7|0|G4|0|G8|0|
    *out_red = _mm256_blend_epi16(red_alpha, _mm256_set1_epi32(0), 170);   // |R1|0|R5|0|R2|0|R6|0|R3|0|R7|0|R4|0|R8|0|
    *out_alpha = _mm256_srli_epi32(red_alpha, 16);                         // |A1|0|A5|0|A2|0|A6|0|A3|0|A7|0|A4|0|A8|0|
    // clang-format on
}

// Load 8 16bit pixels and split them into 8x32 bit registers.
DP_INLINE void load_aligned_avx2(const DP_Pixel15 src[8], __m256i *out_blue,
                                 __m256i *out_green, __m256i *out_red,
                                 __m256i *out_alpha)
{
    DP_ASSERT(((intptr_t)src) % 32 == 0);

    __m256i source1 = _mm256_load_si256((const __m256i *)src);
    __m256i source2 = _mm256_load_si256((const __m256i *)src + 1);

    shuffle_load_avx2(source1, source2, out_blue, out_green, out_red,
                      out_alpha);
}

// Load 8 16bit pixels and split them into 8x32 bit registers.
DP_INLINE void load_unaligned_avx2(const DP_Pixel15 src[8], __m256i *out_blue,
                                   __m256i *out_green, __m256i *out_red,
                                   __m256i *out_alpha)
{
    __m256i source1 = _mm256_loadu_si256((const __m256i *)src);
    __m256i source2 = _mm256_loadu_si256((const __m256i *)src + 1);

    shuffle_load_avx2(source1, source2, out_blue, out_green, out_red,
                      out_alpha);
}


#pragma endregion

#pragma region store
DP_INLINE void shuffle_store_avx2(__m256i blue, __m256i green, __m256i red,
                                  __m256i alpha, __m256i *out1, __m256i *out2)
{
    // clang-format off
    __m256i blue_green = _mm256_blend_epi16(blue, _mm256_slli_si256(green, 2), 170); // |B1|G1|B5|G5|B2|G2|B6|G6|B3|G3|B7|G7|B4|G4|B8|G8|
    __m256i red_alpha = _mm256_blend_epi16(red, _mm256_slli_si256(alpha, 2), 170);   // |R1|A1|R5|A5|R2|A2|R6|A6|R3|A3|R7|A7|R4|A4|R8|A8|

    __m256i lo = _mm256_unpacklo_epi32(blue_green, red_alpha);                       // |B1|G1|R1|A1|B5|G5|R5|A5|B3|G3|R3|A3|B7|G7|R7|A7|
    __m256i hi = _mm256_unpackhi_epi32(blue_green, red_alpha);                       // |B2|G2|R2|A2|B6|G6|R6|A6|B4|G4|R4|A4|B8|G8|R8|A8|

    *out1 = _mm256_unpacklo_epi64(lo, hi);                                           // |B1|G1|R1|A1|B2|G2|R2|A2|B3|G3|R3|A3|B4|G4|R4|A4|
    *out2 = _mm256_unpackhi_epi64(lo, hi);                                           // |B5|G5|R5|A5|B6|G6|R6|A6|B7|G7|R7|A7|B8|G8|R8|A8|
    // clang-format on
}
// Store 8x32 bit registers into 8 16bit pixels.
DP_INLINE void store_aligned_avx2(__m256i blue, __m256i green, __m256i red,
                                  __m256i alpha, DP_Pixel15 dest[8])
{
    DP_ASSERT(((intptr_t)dest) % 32 == 0);

    __m256i out1, out2;
    shuffle_store_avx2(blue, green, red, alpha, &out1, &out2);

    _mm256_store_si256((__m256i *)dest, out1);
    _mm256_store_si256((__m256i *)dest + 1, out2);
}
// Store 8x32 bit registers into 8 16bit pixels.
DP_INLINE void store_unaligned_avx2(__m256i blue, __m256i green, __m256i red,
                                    __m256i alpha, DP_Pixel15 dest[8])
{
    __m256i out1, out2;
    shuffle_store_avx2(blue, green, red, alpha, &out1, &out2);

    _mm256_storeu_si256((__m256i *)dest, out1);
    _mm256_storeu_si256((__m256i *)dest + 1, out2);
}
#pragma endregion

DP_INLINE __m256i mul_avx2(__m256i a, __m256i b)
{
    return _mm256_srli_epi32(_mm256_mullo_epi32(a, b), 15);
}

DP_INLINE __m256i sumprods_avx2(__m256i a1, __m256i a2, __m256i b1, __m256i b2)
{
    return _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi32(a1, a2),
                                              _mm256_mullo_epi32(b1, b2)),
                             15);
}
DP_TARGET_END

DP_INLINE uint32_t mul_seq(uint32_t a, uint32_t b)
{
    return (a * b) >> 15;
}

DP_INLINE uint32_t sumprods_seq(uint32_t a1, uint32_t a2, uint32_t b1,
                                uint32_t b2)
{
    return ((a1 * a2) + (b1 * b2)) >> 15;
}
