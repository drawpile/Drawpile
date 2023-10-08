// SPDX-License-Identifier: MIT
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpengine/pixels.h>
#include <dptest_engine.h>


// "Oracle" versions of conversion functions that go via floating point. The
// actual functions used in the application do integer voodoo instead.

#define BIT15_DOUBLE ((double)DP_BIT15)

static uint16_t channel8_to_15_oracle(uint8_t c)
{
    return DP_double_to_uint16(DP_uint8_to_double(c) / 255.0 * BIT15_DOUBLE);
}

static uint8_t channel15_to_8_oracle(uint16_t c)
{
    return DP_double_to_uint8(c / BIT15_DOUBLE * 255.0 + 0.5);
}


static void channel8_to_15(TEST_PARAMS)
{
    for (int i = 0; i <= 255; ++i) {
        uint8_t c = DP_int_to_uint8(i);
        UINT_EQ_OK(DP_channel8_to_15(c), channel8_to_15_oracle(c),
                   "channel8_to_15(%d)", i);
    }
}


static void channel15_to_8(TEST_PARAMS)
{
    for (int i = 0; i <= DP_BIT15; ++i) {
        uint16_t c = DP_int_to_uint16(i);
        UINT_EQ_OK(DP_channel15_to_8(c), channel15_to_8_oracle(c),
                   "channel15_to_8(%d)", i);
    }
}


// Tile conversion uses vector instructions if available.
// Set the DP_CPU_SUPPORT environment variable to switch which one to use.
static void pixels15_to_8_tile(TEST_PARAMS)
{
    static_assert(DP_BIT15 % 4 == 0, "DP_BIT15 divisible by 4");
    static_assert(DP_BIT15 / 4 == DP_TILE_LENGTH * 2,
                  "Exactly two tiles to cover the whole channel range");

    int pixel_count = DP_BIT15 / 4;
    size_t pixel_scount = DP_int_to_size(pixel_count);

    DP_Pixel15 *src = DP_malloc_simd(pixel_scount * sizeof(*src));
    for (int i = 0; i < pixel_count; ++i) {
        src[i] = (DP_Pixel15){
            .b = DP_int_to_uint16(i * 4),
            .g = DP_int_to_uint16(i * 4 + 1),
            .r = DP_int_to_uint16(i * 4 + 2),
            .a = DP_int_to_uint16(i * 4 + 3),
        };
    }

    DP_Pixel8 *dst = DP_malloc_simd(pixel_scount * sizeof(*dst));
    DP_pixels15_to_8_tile(dst, src);
    DP_pixels15_to_8_tile(dst + DP_TILE_LENGTH, src + DP_TILE_LENGTH);

    for (int i = 0; i < DP_BIT15; ++i) {
        uint16_t c = DP_int_to_uint16(i);
        DP_ASSERT(((uint16_t *)src)[i] == c);
        UINT_EQ_OK(((uint8_t *)dst)[i], channel15_to_8_oracle(c),
                   "pixels15_to_8_tile(%d)", i);
    }

    DP_free_simd(dst);
    DP_free_simd(src);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(channel8_to_15);
    REGISTER_TEST(channel15_to_8);
    REGISTER_TEST(pixels15_to_8_tile);
}

int main(int argc, char **argv)
{
    DP_test_main(argc, argv, register_tests, NULL);
}
