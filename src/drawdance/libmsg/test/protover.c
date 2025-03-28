// SPDX-License-Identifier: GPL-3.0-or-later
#include <dpmsg/message.h>
#include <dpmsg/protover.h>
#include <dptest.h>


static void parse_ok(TEST_PARAMS, const char *input, const char *expected)
{
    DP_ProtocolVersion *protover = DP_protocol_version_parse(input);
    char *actual = protover ? DP_protocol_version_to_string(protover) : "NULL";
    const char *input_quote = input ? "'" : "";
    const char *input_string = input ? input : "NULL";
    const char *expected_quote = expected ? "'" : "";
    const char *expected_string = expected ? expected : "NULL";
    STR_EQ_OK(actual, expected_string, "parse %s%s%s gives %s%s%s", input_quote,
              input_string, input_quote, expected_quote, expected_string,
              expected_quote);
    if (protover) {
        DP_free(actual);
        DP_protocol_version_free(protover);
    }
}

static void parse(TEST_PARAMS)
{
    parse_ok(TEST_ARGS, NULL, NULL);
    parse_ok(TEST_ARGS, "", NULL);
    parse_ok(TEST_ARGS, DP_PROTOCOL_VERSION, DP_PROTOCOL_VERSION);
    parse_ok(TEST_ARGS, "namespace:01.00000100.0000000", "namespace:1.100.0");
    parse_ok(TEST_ARGS, "a:0.0.0", "a:0.0.0");
    parse_ok(TEST_ARGS, "a.0.0.0", NULL);
    parse_ok(TEST_ARGS, "a:0:0:0", NULL);
    parse_ok(TEST_ARGS, "A:1.2.3", NULL);
    parse_ok(TEST_ARGS, ":..", NULL);
    parse_ok(TEST_ARGS, "b:..", NULL);
    parse_ok(TEST_ARGS, "c:1..", NULL);
    parse_ok(TEST_ARGS, "d:1.2.", NULL);
    parse_ok(TEST_ARGS, "e:1.2.3", "e:1.2.3");
    parse_ok(TEST_ARGS, "f:1.2.3.", NULL);
    parse_ok(TEST_ARGS, "g:1.2.3.4", NULL);
    parse_ok(TEST_ARGS,
             "toobig:1.2.99999999999999999999999999999999999999999999999999999",
             NULL);
}


static void compare_check(TEST_PARAMS, const char *title,
                          DP_ProtocolVersion *protover, const char *ns,
                          int server, int major, int minor, bool current,
                          bool future, bool requires_sid,
                          DP_ProtocolCompatibility compatibility)
{
    OK(DP_protocol_version_valid(protover), "%s is valid", title);
    STR_EQ_OK(DP_protocol_version_ns(protover), ns, "%s namespace", title);
    INT_EQ_OK(DP_protocol_version_server(protover), server, "%s server", title);
    INT_EQ_OK(DP_protocol_version_major(protover), major, "%s major", title);
    INT_EQ_OK(DP_protocol_version_minor(protover), minor, "%s minor", title);

    if (current) {
        OK(DP_protocol_version_is_current(protover), "%s is current", title);
    }
    else {
        NOK(DP_protocol_version_is_current(protover), "%s is not current",
            title);
    }

    if (future) {
        OK(DP_protocol_version_is_future(protover), "%s is future", title);
    }
    else {
        NOK(DP_protocol_version_is_future(protover), "%s is not future", title);
    }

    if (requires_sid) {
        OK(DP_protocol_version_should_have_system_id(protover),
           "%s should have system id", title);
    }
    else {
        NOK(DP_protocol_version_should_have_system_id(protover),
            "%s does not need system id", title);
    }

    INT_EQ_OK(DP_protocol_version_client_compatibility(protover), compatibility,
              "%s compatibility is %d", title, (int)compatibility);

    DP_ProtocolVersion *other =
        DP_protocol_version_new(ns, server, major, minor);
    OK(DP_protocol_version_equals(protover, protover), "%s == itself", title);
    OK(DP_protocol_version_equals(protover, other), "%s == copy", title);
    OK(DP_protocol_version_greater_or_equal(protover, protover), "%s >= copy",
       title);
    OK(DP_protocol_version_greater_or_equal(protover, other), "%s >= copy",
       title);
    UINT_EQ_OK(DP_protocol_version_as_integer(protover),
               DP_protocol_version_as_integer(protover),
               "%s as integer == itself", title);
    UINT_EQ_OK(DP_protocol_version_as_integer(protover),
               DP_protocol_version_as_integer(other), "%s as integer == copy",
               title);
    DP_protocol_version_free(other);
}

static void compare_versions(TEST_PARAMS, const char *a_title,
                             const char *b_title, DP_ProtocolVersion *a,
                             DP_ProtocolVersion *b, bool geq)
{
    NOK(DP_protocol_version_equals(a, b), "%s != %s", a_title, b_title);
    if (geq) {
        OK(DP_protocol_version_greater_or_equal(a, b), "%s >= %s", a_title,
           b_title);
        OK(DP_protocol_version_as_integer(a)
               >= DP_protocol_version_as_integer(b),
           "%s >= %s as integers", a_title, b_title);
    }
    else {
        NOK(DP_protocol_version_greater_or_equal(a, b), "%s < %s", a_title,
            b_title);
        NOK(DP_protocol_version_as_integer(a)
                >= DP_protocol_version_as_integer(b),
            "%s < %s as integers", a_title, b_title);
    }
}

static void compare(TEST_PARAMS)
{
    DP_ProtocolVersion *current = DP_protocol_version_new_current();
    compare_check(TEST_ARGS, "current", current, DP_PROTOCOL_VERSION_NAMESPACE,
                  DP_PROTOCOL_VERSION_SERVER, DP_PROTOCOL_VERSION_MAJOR,
                  DP_PROTOCOL_VERSION_MINOR, true, false, true,
                  DP_PROTOCOL_COMPATIBILITY_COMPATIBLE);

    DP_ProtocolVersion *past_incompatible =
        DP_protocol_version_new("dp", 4, 20, 1);
    compare_check(TEST_ARGS, "dp:4.20.1", past_incompatible, "dp", 4, 20, 1,
                  false, false, false, DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE);

    DP_ProtocolVersion *minor_future = DP_protocol_version_new(
        DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER,
        DP_PROTOCOL_VERSION_MAJOR, DP_PROTOCOL_VERSION_MINOR + 1);
    compare_check(TEST_ARGS, "minor+1", minor_future,
                  DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER,
                  DP_PROTOCOL_VERSION_MAJOR, DP_PROTOCOL_VERSION_MINOR + 1,
                  false, true, true,
                  DP_PROTOCOL_COMPATIBILITY_MINOR_INCOMPATIBILITY);

    DP_ProtocolVersion *major_future = DP_protocol_version_new(
        DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER,
        DP_PROTOCOL_VERSION_MAJOR + 1, DP_PROTOCOL_VERSION_MINOR);
    compare_check(TEST_ARGS, "major+1", major_future,
                  DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER,
                  DP_PROTOCOL_VERSION_MAJOR + 1, DP_PROTOCOL_VERSION_MINOR,
                  false, true, true, DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE);

    DP_ProtocolVersion *server_future = DP_protocol_version_new(
        DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER + 1,
        DP_PROTOCOL_VERSION_MAJOR, DP_PROTOCOL_VERSION_MINOR);
    compare_check(TEST_ARGS, "server+1", server_future,
                  DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER + 1,
                  DP_PROTOCOL_VERSION_MAJOR, DP_PROTOCOL_VERSION_MINOR, false,
                  true, true,
                  // Server portion does not affect client compatibility.
                  // In practice, it will certainly cause a major bump too.
                  DP_PROTOCOL_COMPATIBILITY_COMPATIBLE);

    DP_ProtocolVersion *other_namespace = DP_protocol_version_new(
        "qq", DP_PROTOCOL_VERSION_SERVER, DP_PROTOCOL_VERSION_MAJOR,
        DP_PROTOCOL_VERSION_MINOR);
    compare_check(TEST_ARGS, "other namespace", other_namespace, "qq",
                  DP_PROTOCOL_VERSION_SERVER, DP_PROTOCOL_VERSION_MAJOR,
                  DP_PROTOCOL_VERSION_MINOR, false, false, false,
                  DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE);

    compare_versions(TEST_ARGS, "server+1", "major+1", server_future,
                     major_future, true);
    compare_versions(TEST_ARGS, "server+1", "minor+1", server_future,
                     minor_future, true);
    compare_versions(TEST_ARGS, "server+1", "current", server_future, current,
                     true);
    compare_versions(TEST_ARGS, "server+1", "past incompatible", server_future,
                     past_incompatible, true);

    compare_versions(TEST_ARGS, "major+1", "server+1", major_future,
                     server_future, false);
    compare_versions(TEST_ARGS, "major+1", "minor+1", major_future,
                     minor_future, true);
    compare_versions(TEST_ARGS, "major+1", "current", major_future, current,
                     true);
    compare_versions(TEST_ARGS, "major+1", "past incompatible", major_future,
                     past_incompatible, true);

    compare_versions(TEST_ARGS, "minor+1", "server+1", minor_future,
                     server_future, false);
    compare_versions(TEST_ARGS, "minor+1", "minor+1", minor_future,
                     major_future, false);
    compare_versions(TEST_ARGS, "minor+1", "current", minor_future, current,
                     true);
    compare_versions(TEST_ARGS, "minor+1", "past incompatible", minor_future,
                     past_incompatible, true);

    compare_versions(TEST_ARGS, "current", "server+1", current, server_future,
                     false);
    compare_versions(TEST_ARGS, "current", "major+1", current, major_future,
                     false);
    compare_versions(TEST_ARGS, "current", "minor+1", current, minor_future,
                     false);
    compare_versions(TEST_ARGS, "current", "past incompatible", current,
                     past_incompatible, true);

    compare_versions(TEST_ARGS, "past incompatible", "server+1",
                     past_incompatible, server_future, false);
    compare_versions(TEST_ARGS, "past incompatible", "major+1",
                     past_incompatible, major_future, false);
    compare_versions(TEST_ARGS, "past incompatible", "minor+1",
                     past_incompatible, minor_future, false);
    compare_versions(TEST_ARGS, "past incompatible", "current",
                     past_incompatible, current, false);

    DP_protocol_version_free(other_namespace);
    DP_protocol_version_free(server_future);
    DP_protocol_version_free(major_future);
    DP_protocol_version_free(minor_future);
    DP_protocol_version_free(past_incompatible);
    DP_protocol_version_free(current);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(parse);
    REGISTER_TEST(compare);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
