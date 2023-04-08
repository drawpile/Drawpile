/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <dpcommon/base64.h>
#include <dptest.h>


static void encode_ok(TEST_PARAMS, const void *in, size_t in_length,
                      const char *expected)
{
    size_t actual_length;
    char *encoded = DP_base64_encode(in, in_length, &actual_length);
    STR_LEN_EQ_OK(encoded, actual_length, expected, strlen(expected),
                  "correct encoded value");
    INT_EQ_OK(encoded[actual_length], '\0', "properly null-terminated");
    DP_free(encoded);
}

static void encode_null(TEST_PARAMS)
{
    encode_ok(TEST_ARGS, NULL, 0, "");
}

static void encode_zero(TEST_PARAMS)
{
    encode_ok(TEST_ARGS, "", 0, "");
}

static void encode_one(TEST_PARAMS)
{
    encode_ok(TEST_ARGS, "a", 1, "YQ==");
}

static void encode_two(TEST_PARAMS)
{
    encode_ok(TEST_ARGS, "bc", 2, "YmM=");
}

static void encode_three(TEST_PARAMS)
{
    encode_ok(TEST_ARGS, "def", 3, "ZGVm");
}

static void encode_data(TEST_PARAMS)
{
    static const unsigned char data[] = {
        129, 244, 184, 213, 83,  88,  89,  102, 14,  54,  141, 210, 189,
        245, 215, 235, 72,  233, 15,  213, 73,  79,  233, 53,  31,  186,
        89,  215, 5,   194, 82,  57,  23,  210, 117, 0,   57,  105, 198,
        90,  34,  71,  51,  162, 77,  127, 118, 243, 63,  146, 222, 30,
        146, 19,  3,   221, 158, 210, 107, 99,  63,  118, 169, 154, 11,
        189, 203, 160, 100, 195, 124, 147, 133, 105, 147, 46,  135, 107,
        89,  96,  241, 179, 239, 128, 155, 168, 227, 223, 153, 122, 180,
        234, 37,  215, 157, 173, 68,  150, 29,  51,
    };
    encode_ok(TEST_ARGS, data, sizeof(data),
              "gfS41VNYWWYONo3SvfXX60jpD9VJT+"
              "k1H7pZ1wXCUjkX0nUAOWnGWiJHM6JNf3bzP5LeHpITA92e0mtjP3apmgu9y6B"
              "kw3yThWmTLodrWWDxs++Am6jj35l6tOol152tRJYdMw==");
}


static void decode_ok(TEST_PARAMS, const char *in, const void *expected_out,
                      size_t expected_length)
{
    size_t in_length = in ? strlen(in) : 0;
    size_t out_length = DP_base64_decode_length(in, in_length);
    if (UINT_EQ_OK(out_length, expected_length, "output length matches")
        && out_length != 0) {
        unsigned char *out = DP_malloc_zeroed(out_length);
        DP_base64_decode(in, in_length, out, out_length);
        STR_LEN_EQ_OK((const char *)out, out_length, expected_out,
                      expected_length, "output content matches");
        DP_free(out);
    }
}

static void decode_null(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, NULL, NULL, 0);
}

static void decode_empty(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "", NULL, 0);
}

static void decode_pad_two(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "YQ==", "a", 1);
}

static void decode_pad_two_missing_equals(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "YQ", "a", 1);
}

static void decode_pad_two_single_equals(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "YQ=", "a", 1);
}

static void decode_pad_one(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "YmM=", "bc", 2);
}

static void decode_pad_one_missing_equals(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "YmM", "bc", 2);
}

static void decode_pad_one_extra_equals(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "YmM==", "bc", 2);
}

static void decode_pad_none(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "ZGVm", "def", 3);
}

static void decode_invalid_length(TEST_PARAMS)
{
    decode_ok(TEST_ARGS, "ZGVm1", "def", 3);
}

static void decode_data(TEST_PARAMS)
{
    decode_ok(TEST_ARGS,
              "WWVhaCwgd2hlbiBJIHRoaW5rIG9mIE1\n"
              "hdXJlZW4gSSB0aGluayBvZiB0d28gdG\n"
              "hpbmdzOgphc3BoYWx0Li4uCi4uLmFuZ\n"
              "CB0cm91YmxlLg==",
              "Yeah, when I think of Maureen I think of two things:\n"
              "asphalt...\n...and trouble.",
              79);
}


static void encode_decode_random(TEST_PARAMS)
{
    size_t plain_length = DP_int_to_size(1 + rand() % 9999);
    NOTE("Encoding %zu random bytes", plain_length);
    unsigned char *plain = DP_malloc(plain_length);
    for (size_t i = 0; i < plain_length; ++i) {
        plain[i] = DP_int_to_uchar(rand() % 256);
    }

    size_t encoded_length;
    char *encoded = DP_base64_encode(plain, plain_length, &encoded_length);

    size_t decoded_length = DP_base64_decode_length(encoded, encoded_length);
    if (UINT_EQ_OK(decoded_length, plain_length, "decoded length matches")) {
        unsigned char *decoded = DP_malloc_zeroed(decoded_length);
        DP_base64_decode(encoded, encoded_length, decoded, decoded_length);
        OK(memcmp(decoded, plain, plain_length) == 0,
           "decoded content matches");
        DP_free(decoded);
    }

    DP_free(encoded);
    DP_free(plain);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(encode_null);
    REGISTER_TEST(encode_zero);
    REGISTER_TEST(encode_one);
    REGISTER_TEST(encode_two);
    REGISTER_TEST(encode_three);
    REGISTER_TEST(encode_data);
    REGISTER_TEST(decode_null);
    REGISTER_TEST(decode_empty);
    REGISTER_TEST(decode_pad_two);
    REGISTER_TEST(decode_pad_two_missing_equals);
    REGISTER_TEST(decode_pad_two_single_equals);
    REGISTER_TEST(decode_pad_one);
    REGISTER_TEST(decode_pad_one_missing_equals);
    REGISTER_TEST(decode_pad_one_extra_equals);
    REGISTER_TEST(decode_pad_none);
    REGISTER_TEST(decode_invalid_length);
    REGISTER_TEST(decode_data);
    REGISTER_TEST(encode_decode_random);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
