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
#include "dptest.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>

#define INCLUDE_PREFIX "--include="
#define EXCLUDE_PREFIX "--exclude="

struct DP_TestRunParams {
    // These are NULL if empty or contain multiple null-terminated non-empty
    // strings followed by an empty null-terminated string marking the end.
    char *includes, *excludes;
};


void DP_test_register(DP_TestRegistry *R, const char *name, DP_TestFn test_fn,
                      void *user)
{
    if (R->used == R->capacity) {
        R->capacity = DP_max_int(8, R->capacity * 2);
        R->tests = DP_realloc(R->tests,
                              sizeof(*R->tests) * DP_int_to_size(R->capacity));
    }
    DP_Test *test = &R->tests[R->used++];
    size_t name_len = strlen(name);
    test->name = malloc(name_len + 1);
    memcpy(test->name, name, name_len + 1);
    test->test_fn = test_fn;
    test->user = user;
}


static DP_Output *append_test_name(void ***buffer_ptr, DP_Output *out,
                                   const char *arg, size_t length)
{
    if (!out) {
        out = DP_mem_output_new(64, false, buffer_ptr, NULL);
        if (!out) {
            DP_panic("Error creating mem output: %s", DP_error());
        }
    }
    DP_output_write(out, arg, length);
    DP_output_write(out, "\0", 1);
    return out;
}

static DP_Output *parse_comma_separated_test_names(void ***buffer_ptr,
                                                   DP_Output *out,
                                                   const char *arg)
{
    size_t start = 0;
    size_t total_length = strlen(arg);
    for (size_t i = 0; i < total_length; ++i) {
        if (arg[i] == ',') {
            if (start != i) {
                out = append_test_name(buffer_ptr, out, arg + start, i - start);
            }
            start = i + 1;
        }
    }
    if (start < total_length) {
        out = append_test_name(buffer_ptr, out, arg + start,
                               total_length - start);
    }
    return out;
}

static bool parse_run_params(struct DP_TestRunParams *params, int argc,
                             char **argv, int offset)
{
    void **includes_buffer_ptr = NULL;
    void **excludes_buffer_ptr = NULL;
    DP_Output *includes_out = NULL;
    DP_Output *excludes_out = NULL;
    bool error = false;

    for (int i = offset; i < argc; ++i) {
        const char *arg = argv[i];
        if (strncmp(arg, INCLUDE_PREFIX, strlen(INCLUDE_PREFIX)) == 0) {
            includes_out = parse_comma_separated_test_names(
                &includes_buffer_ptr, includes_out,
                arg + strlen(INCLUDE_PREFIX));
        }
        else if (strncmp(arg, EXCLUDE_PREFIX, strlen(EXCLUDE_PREFIX)) == 0) {
            excludes_out = parse_comma_separated_test_names(
                &excludes_buffer_ptr, excludes_out,
                arg + strlen(EXCLUDE_PREFIX));
        }
        else {
            DP_warn("Unknown argument '%s'", arg);
            error = true;
        }
    }

    char *includes;
    if (includes_out) {
        DP_output_write(includes_out, "\0", 1);
        includes = *includes_buffer_ptr;
        DP_free(includes_out);
    }
    else {
        includes = NULL;
    }

    char *excludes;
    if (excludes_out) {
        DP_output_write(excludes_out, "\0", 1);
        excludes = *excludes_buffer_ptr;
        DP_free(excludes_out);
    }
    else {
        excludes = NULL;
    }

    if (error) {
        DP_free(includes);
        DP_free(excludes);
        return false;
    }
    else {
        params->includes = includes;
        params->excludes = excludes;
        return true;
    }
}

static bool is_in_param(const char *param, const char *name)
{
    for (size_t offset = 0; strlen(param + offset) != 0;
         offset += strlen(param + offset) + 1) {
        if (strcmp(param + offset, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool should_run_test(struct DP_TestRunParams *params, DP_Test *test)
{
    const char *name = test->name;
    bool is_included = !params->includes || is_in_param(params->includes, name);
    bool is_excluded = params->excludes && is_in_param(params->excludes, name);
    return is_included && !is_excluded;
}

static void run_test(DP_TestContext *T)
{
    NOTE("Running test %s", T->test->name);
    T->test->test_fn(T);
}

static void print_done_testing(FILE *fp, long long total)
{
    fprintf(fp, "1..%lld\n", total);
    fflush(fp);
}


static int command_run(int argc, char **argv, int offset, DP_TestRegistry *R,
                       void *user)
{
    struct DP_TestRunParams params;
    if (parse_run_params(&params, argc, argv, offset)) {
        DP_TestContext context = {NULL, "«", "»", stdout, stderr, 0, 0, user};
        for (int i = 0; i < R->used; ++i) {
            DP_Test *test = &R->tests[i];
            if (should_run_test(&params, test)) {
                context.test = test;
                run_test(&context);
            }
        }

        print_done_testing(stdout, context.total);
        DP_free(params.includes);
        DP_free(params.excludes);

        long long failures = context.total - context.passed;
        return failures > 254 ? 254 : (int)failures;
    }
    else {
        DP_warn("run: error parsing arguments");
        return 255;
    }
}

static int command_list(int argc, DP_UNUSED char **argv, int offset,
                        DP_TestRegistry *R, DP_UNUSED void *user)
{
    if (argc <= offset) {
        for (int i = 0; i < R->used; ++i) {
            puts(R->tests[i].name);
        }
        return 0;
    }
    else {
        DP_warn("list: this command does not accept any arguments");
        return 255;
    }
}

static int command_help(int argc, char **argv, DP_UNUSED int offset,
                        DP_UNUSED DP_TestRegistry *R, DP_UNUSED void *user)
{
    const char *invoker = argc > 0 ? argv[0] : "dptest";
    printf("\n");
    printf("%s: Drawdance TAP tests\n", invoker);
    printf("\n");
    printf("Usage:\n");
    printf("    %s [run] [--include=t1[,t2...]] [--exclude=t1[,t2...]]\n",
           invoker);
    printf("        Run tests. Will run all tests by default. Use includes\n");
    printf("        to specify which tests to run and excludes to specify\n");
    printf("        which tests not to run.\n");
    printf("\n");
    printf("    %s list\n", invoker);
    printf("        List tests, one per line.\n");
    printf("\n");
    printf("    %s help\n", invoker);
    printf("        Show this help.\n");
    printf("\n");
    return 0;
}

static int command_unknown(DP_UNUSED int argc, DP_UNUSED char **argv,
                           DP_UNUSED int offset, DP_UNUSED DP_TestRegistry *R,
                           DP_UNUSED void *user)
{
    return 255;
}

static void free_registry(DP_TestRegistry *R)
{
    for (int i = 0; i < R->used; ++i) {
        DP_free(R->tests[i].name);
    }
    DP_free(R->tests);
}

int DP_test_main(int argc, char **argv, DP_TestRegisterFn register_fn,
                 void *user)
{
    DP_TestRegistry registry = {0, 0, NULL, user};
    register_fn(&registry);

    int (*command)(int, char **, int, DP_TestRegistry *, void *) = NULL;
    if (argc > 1) {
        const char *arg = argv[1];
        if (strcmp(arg, "run") == 0) {
            command = command_run;
        }
        else if (strcmp(arg, "list") == 0) {
            command = command_list;
        }
        else if (strcmp(arg, "help") == 0) {
            command = command_help;
        }
        else if (arg[0] != '-') {
            DP_warn("Unknown command '%s'", arg);
            command = command_unknown;
        }
    }

    // Respond to cries for help in any argument.
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-help") == 0
            || strcmp(arg, "-h") == 0 || strcmp(arg, "-?") == 0) {
            command = command_help;
        }
    }

    int offset;
    if (command) {
        offset = 2;
    }
    else {
        offset = 1;
        command = command_run;
    }

    int result = command(argc, argv, offset, &registry, user);
    free_registry(&registry);
    return result;
}


static void write_comment_line(FILE *fp, const char *s, size_t length)
{
    fputs("# ", fp);
    fwrite(s, 1, length, fp);
    fputc('\n', fp);
}

static void write_comment(FILE *fp, const char *fmt, va_list ap)
{
    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *mem = DP_mem_output_new(64, true, &buffer_ptr, &size_ptr);
    DP_output_vformat(mem, fmt, ap);

    char *buffer = *buffer_ptr;
    size_t size = *size_ptr;

    size_t start = 0;
    size_t length = 0;
    for (size_t i = 0; i < size; ++i) {
        if (buffer[i] == '\n') {
            write_comment_line(fp, buffer + start, length);
            start = i + 1;
            length = 0;
        }
        else {
            ++length;
        }
    }

    if (length != 0) {
        write_comment_line(fp, buffer + start, length);
    }

    fflush(fp);
    DP_output_free(mem);
}

void DP_test_note(DP_TestContext *T, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_comment(T->out, fmt, ap);
    va_end(ap);
}

void DP_test_diag(DP_TestContext *T, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_comment(T->err, fmt, ap);
    va_end(ap);
}


bool DP_test_vok(DP_TestContext *T, const char *file, int line, bool value,
                 const char *fmt, va_list ap)
{
    long long n = ++T->total;
    if (value) {
        ++T->passed;
    }
    fprintf(T->out, "%s %lld - ", value ? "ok" : "not ok", n);
    vfprintf(T->out, fmt, ap);
    fputc('\n', T->out);
    fflush(T->out);
    if (!value) {
        DP_test_diag(T, "Failure in test '%s' (%s, line %d)", T->test->name,
                     &file[DP_PROJECT_DIR_LENGTH], line);
    }
    return value;
}

bool DP_test_ok(DP_TestContext *T, const char *file, int line, bool value,
                const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, value, fmt, ap);
    va_end(ap);
    return result;
}

bool DP_test_pass(DP_TestContext *T, const char *file, int line,
                  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, true, fmt, ap);
    va_end(ap);
    return result;
}

bool DP_test_fail(DP_TestContext *T, const char *file, int line,
                  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, false, fmt, ap);
    va_end(ap);
    return result;
}


bool DP_test_int_eq_ok(DP_TestContext *T, const char *file, int line,
                       const char *sa, const char *sb, long long a, long long b,
                       const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, a == b, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_ACTUAL_EXPECTED("%lld", sa, sb, a, b);
    }
    return result;
}

bool DP_test_uint_eq_ok(DP_TestContext *T, const char *file, int line,
                        const char *sa, const char *sb, unsigned long long a,
                        unsigned long long b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, a == b, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_ACTUAL_EXPECTED("%llu", sa, sb, a, b);
    }
    return result;
}

bool DP_test_str_eq_ok(DP_TestContext *T, const char *file, int line,
                       const char *sa, const char *sb, const char *a,
                       const char *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, strcmp(a, b) == 0, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_ACTUAL_EXPECTED("%s", sa, sb, a, b);
    }
    return result;
}

bool DP_test_str_len_eq_ok(DP_TestContext *T, const char *file, int line,
                           const char *sa, const char *salen, const char *sb,
                           const char *sblen, const char *a, size_t alen,
                           const char *b, size_t blen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line,
                              alen == blen && memcmp(a, b, alen) == 0, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_VALUE("actual length", "%zu", salen, alen);
        DIAG_VALUE("actual string", "%.*s", sa, DP_size_to_int(alen), a);
        DIAG_VALUE("expected length", "%zu", sblen, blen);
        DIAG_VALUE("expected string", "%.*s", sb, DP_size_to_int(blen), b);
    }
    return result;
}

bool DP_test_ptr_eq_ok(DP_TestContext *T, const char *file, int line,
                       const char *sa, const char *sb, const void *a,
                       const void *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, a == b, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_ACTUAL_EXPECTED("%p", sa, sb, a, b);
    }
    return result;
}

bool DP_test_not_null_ok(DP_TestContext *T, const char *file, int line,
                         const char *sa, const void *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, a, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_VALUE("actual", "%p", sa, a);
        DIAG("expected non-NULL");
    }
    return result;
}

bool DP_test_null_ok(DP_TestContext *T, const char *file, int line,
                     const char *sa, const void *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = DP_test_vok(T, file, line, !a, fmt, ap);
    va_end(ap);
    if (!result) {
        DIAG_VALUE("actual", "%p", sa, a);
        DIAG("expected NULL");
    }
    return result;
}

bool DP_test_file_eq_ok(DP_TestContext *T, const char *file, int line,
                        const char *sa, const char *sb, const char *a,
                        const char *b, const char *fmt, ...)
{
    size_t length_a;
    unsigned char *content_a = DP_slurp(a, &length_a);
    if (!content_a) {
        va_list ap;
        va_start(ap, fmt);
        DP_test_vok(T, file, line, false, fmt, ap);
        va_end(ap);
        DIAG("reading actual path failed: %s", DP_error());
        DIAG_VALUE("actual path", "%s", sa, a);
        DIAG_VALUE("expected path", "%s", sb, b);
        return false;
    }

    size_t length_b;
    unsigned char *content_b = DP_slurp(b, &length_b);
    if (!content_b) {
        DP_free(content_a);
        va_list ap;
        va_start(ap, fmt);
        DP_test_vok(T, file, line, false, fmt, ap);
        va_end(ap);
        DIAG("reading expected path failed: %s", DP_error());
        DIAG_VALUE("actual path", "%s", sa, a);
        DIAG_VALUE("expected path", "%s", sb, b);
        return false;
    }

    if (length_a != length_b) {
        va_list ap;
        va_start(ap, fmt);
        DP_test_vok(T, file, line, false, fmt, ap);
        va_end(ap);
        DIAG("file content lengths differ\n"
             "actual length:   %zu\n"
             "expected length: %zu",
             length_a, length_b);
        DIAG_VALUE("actual path", "%s", sa, a);
        DIAG_VALUE("expected path", "%s", sb, b);
        DP_free(content_a);
        DP_free(content_b);
        return false;
    }

    for (size_t i = 0; i < length_a; ++i) {
        unsigned char byte_a = content_a[i];
        unsigned char byte_b = content_b[i];
        if (byte_a != byte_b) {
            DIAG("file bytes differ at %zu\n"
                 "actual byte:   %02x\n"
                 "expected byte: %02x",
                 i, DP_uchar_to_uint(byte_a), DP_uchar_to_uint(byte_b));
            DIAG_VALUE("actual path", "%s", sa, a);
            DIAG_VALUE("expected path", "%s", sb, b);
            DP_free(content_a);
            DP_free(content_b);
            return false;
        }
    }

    DP_free(content_a);
    DP_free(content_b);

    va_list ap;
    va_start(ap, fmt);
    DP_test_vok(T, file, line, true, fmt, ap);
    va_end(ap);
    return true;
}
