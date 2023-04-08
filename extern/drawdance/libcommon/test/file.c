/*
 * Copyright (c) 2023 askmeaboutloom
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
#include <dpcommon/file.h>
#include <dptest.h>


struct DP_UniquePathTestParams {
    TEST_FIELD_DECL;
    int count;
};

static bool accept(void *user, const char *path)
{
    struct DP_UniquePathTestParams *params = user;
    TEST_UNPACK(*params);
    int count = params->count++;
    NOTE("Path %d: '%s'", count, path);
    return count == 3;
}

static void check_unique_path(TEST_PARAMS, const char *input, int max,
                              const char *expected, const char *message)
{
    struct DP_UniquePathTestParams params = {TEST_FIELD_INIT, 0};
    char *path = DP_file_path_unique(input, max, accept, &params);
    if (expected) {
        STR_EQ_OK(path, expected, "unique path: %s", message);
    }
    else {
        NULL_OK(path, "unique path: %s", message);
    }
    DP_free(path);
}

static void path_unique(TEST_PARAMS)
{
    check_unique_path(TEST_ARGS, "test.txt", 3, "test (3).txt",
                      "with extension");
    check_unique_path(TEST_ARGS, "test.file.txt", 3, "test.file (3).txt",
                      "with multiple extensions");
    check_unique_path(TEST_ARGS, "test", 99, "test (3)", "without extension");
    check_unique_path(TEST_ARGS, "test (1).txt", 99, "test (5).txt",
                      "with extension and suffix already present");
    check_unique_path(TEST_ARGS, "test (9)", 99, "test (13)",
                      "without extension and suffix already present");
    check_unique_path(TEST_ARGS, "te.st (2) file (3).txt", 99,
                      "te.st (2) file (7).txt",
                      "with multiple extension and suffixes already present");
    check_unique_path(TEST_ARGS, ".config", 99, ".config (3)",
                      "path starting with dot");
    check_unique_path(TEST_ARGS, ".config (12)", 99, ".config (16)",
                      "path starting with dot and suffix already present");
    check_unique_path(TEST_ARGS, "", 99, " (3)", "empty path");
    check_unique_path(TEST_ARGS, " (1000)", 2000, " (1004)",
                      "empty path with suffix already present");
    check_unique_path(TEST_ARGS, "test.txt", 2, NULL, "running out of tries");
    check_unique_path(TEST_ARGS, "test (99).txt", 99, NULL,
                      "running out of tries with suffix already present");
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(path_unique);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
