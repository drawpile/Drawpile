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


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(encode_null);
    REGISTER_TEST(encode_zero);
    REGISTER_TEST(encode_one);
    REGISTER_TEST(encode_two);
    REGISTER_TEST(encode_three);
    REGISTER_TEST(encode_data);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
