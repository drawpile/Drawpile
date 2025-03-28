// SPDX-License-Identifier: GPL-3.0-or-later
#include "protover.h"
#include "message.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


struct DP_ProtocolVersion {
    int server;
    int major;
    int minor;
    char ns[];
};

static DP_ProtocolVersion *make_protocol_version(size_t ns_len, const char *ns,
                                                 int server, int major,
                                                 int minor)
{
    size_t ns_size = ns_len + 1;
    DP_ProtocolVersion *protover =
        DP_malloc(DP_FLEX_SIZEOF(DP_ProtocolVersion, ns, ns_size));
    protover->server = server;
    protover->major = major;
    protover->minor = minor;
    if (ns_len != 0) {
        memcpy(protover->ns, ns, ns_len);
    }
    protover->ns[ns_len] = '\0';
    return protover;
}

DP_ProtocolVersion *DP_protocol_version_new(const char *ns, int server,
                                            int major, int minor)
{
    return make_protocol_version(ns ? strlen(ns) : 0, ns, server, major, minor);
}

DP_ProtocolVersion *DP_protocol_version_new_major_minor(int major, int minor)
{
    return DP_protocol_version_new(DP_PROTOCOL_VERSION_NAMESPACE,
                                   DP_PROTOCOL_VERSION_SERVER, major, minor);
}

DP_ProtocolVersion *DP_protocol_version_new_current(void)
{
    return DP_protocol_version_new(
        DP_PROTOCOL_VERSION_NAMESPACE, DP_PROTOCOL_VERSION_SERVER,
        DP_PROTOCOL_VERSION_MAJOR, DP_PROTOCOL_VERSION_MINOR);
}

DP_ProtocolVersion *
DP_protocol_version_new_clone(const DP_ProtocolVersion *protover)
{
    return protover ? DP_protocol_version_new(protover->ns, protover->server,
                                              protover->major, protover->minor)
                    : NULL;
}

static bool is_a_to_z(char c)
{
    switch (c) {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
        return true;
    default:
        return false;
    }
}

static bool parse_version_number(const char *start, const char *expected_end,
                                 int *out_value)
{
    if (expected_end && start != expected_end) {
        char *end;
        long value = strtol(start, &end, 10);
        // On Windows, strtol returns LONG_MAX on error. Since we don't need
        // billions of version numbers, we just treat it as invalid everywhere.
        if (end == expected_end && value >= 0 && value < LONG_MAX
            && value <= INT_MAX) {
            *out_value = DP_long_to_int(value);
            return true;
        }
    }
    return false;
}

DP_ProtocolVersion *DP_protocol_version_parse(const char *s)
{
    if (!s) {
        return NULL;
    }

    size_t len = strlen(s);
    size_t ns_len = 0;

    while (ns_len < len) {
        char c = s[ns_len];
        if (is_a_to_z(c)) {
            ++ns_len;
        }
        else if (c == ':') {
            break;
        }
        else {
            return NULL;
        }
    }

    if (ns_len == 0) {
        return NULL;
    }

    const char *server_start = s + ns_len + 1;
    const char *server_end = strchr(server_start, '.');
    int server;
    if (!parse_version_number(server_start, server_end, &server)) {
        return NULL;
    }

    const char *major_start = server_end + 1;
    const char *major_end = strchr(major_start, '.');
    int major;
    if (!parse_version_number(major_start, major_end, &major)) {
        return NULL;
    }

    const char *minor_start = major_end + 1;
    const char *minor_end = s + len;
    int minor;
    if (!parse_version_number(minor_start, minor_end, &minor)) {
        return NULL;
    }

    return make_protocol_version(ns_len, s, server, major, minor);
}

void DP_protocol_version_free(DP_ProtocolVersion *protover)
{
    DP_free(protover);
}

char *DP_protocol_version_to_string(const DP_ProtocolVersion *protover)
{
    return DP_format("%s:%d.%d.%d", protover->ns, protover->server,
                     protover->major, protover->minor);
}

static bool is_ns_valid(const char *ns)
{
    if (ns) {
        const char *s = ns;
        while (*s != '\0' && is_a_to_z(*s)) {
            ++s;
        }
        return s != ns && *s == '\0';
    }
    else {
        return false;
    }
}

bool DP_protocol_version_valid(const DP_ProtocolVersion *protover)
{
    return protover && protover->server >= 0 && protover->major >= 0
        && protover->minor >= 0 && is_ns_valid(protover->ns);
}

bool DP_protocol_version_is_current(const DP_ProtocolVersion *protover)
{
    return protover && DP_str_equal(protover->ns, DP_PROTOCOL_VERSION_NAMESPACE)
        && protover->server == DP_PROTOCOL_VERSION_SERVER
        && protover->major == DP_PROTOCOL_VERSION_MAJOR
        && protover->minor == DP_PROTOCOL_VERSION_MINOR;
}

bool DP_protocol_version_is_future(const DP_ProtocolVersion *protover)
{
    return protover && DP_str_equal(protover->ns, DP_PROTOCOL_VERSION_NAMESPACE)
        && (protover->server > DP_PROTOCOL_VERSION_SERVER
            || (protover->server == DP_PROTOCOL_VERSION_SERVER
                && (protover->major > DP_PROTOCOL_VERSION_MAJOR
                    || (protover->major == DP_PROTOCOL_VERSION_MAJOR
                        && protover->minor > DP_PROTOCOL_VERSION_MINOR))));
}

bool DP_protocol_version_is_past(const DP_ProtocolVersion *protover)
{
    return protover && DP_str_equal(protover->ns, DP_PROTOCOL_VERSION_NAMESPACE)
        && (protover->server < DP_PROTOCOL_VERSION_SERVER
            || (protover->server == DP_PROTOCOL_VERSION_SERVER
                && (protover->major < DP_PROTOCOL_VERSION_MAJOR
                    || (protover->major == DP_PROTOCOL_VERSION_MAJOR
                        && protover->minor < DP_PROTOCOL_VERSION_MINOR))));
}

static bool is_at_least_4_24_0(const DP_ProtocolVersion *protover)
{
    return protover && DP_str_equal(protover->ns, DP_PROTOCOL_VERSION_NAMESPACE)
        && (protover->server > 4
            || (protover->server == 4
                && (protover->major > 24
                    || (protover->major == 24 && protover->minor >= 0))));
}

bool DP_protocol_version_should_have_system_id(
    const DP_ProtocolVersion *protover)
{
    return is_at_least_4_24_0(protover);
}

bool DP_protocol_version_should_support_lookup(
    const DP_ProtocolVersion *protover)
{
    return is_at_least_4_24_0(protover);
}

const char *DP_protocol_version_ns(const DP_ProtocolVersion *protover)
{
    return protover ? protover->ns : NULL;
}

int DP_protocol_version_server(const DP_ProtocolVersion *protover)
{
    return protover ? protover->server : -1;
}

int DP_protocol_version_major(const DP_ProtocolVersion *protover)
{
    return protover ? protover->major : -1;
}

int DP_protocol_version_minor(const DP_ProtocolVersion *protover)
{
    return protover ? protover->minor : -1;
}

const char *DP_protocol_version_name(const DP_ProtocolVersion *protover)
{
    if (protover && DP_str_equal(protover->ns, DP_PROTOCOL_VERSION_NAMESPACE)
        && protover->server == 4) {
        if (protover->major == 24) {
            return "2.2.x";
        }
        else if (protover->major == 22 || protover->major == 23) {
            return "2.2.0 beta";
        }
        else if (protover->major == 21 && protover->minor == 2) {
            return "2.1.x";
        }
        else if (protover->major == 20 && protover->minor == 1) {
            return "2.0.x";
        }
    }
    return NULL;
}

DP_ProtocolCompatibility
DP_protocol_version_client_compatibility(const DP_ProtocolVersion *protover)
{
    if (protover && DP_str_equal(protover->ns, DP_PROTOCOL_VERSION_NAMESPACE)) {
        if (protover->major == DP_PROTOCOL_VERSION_MAJOR) {
            if (protover->minor == DP_PROTOCOL_VERSION_MINOR) {
                return DP_PROTOCOL_COMPATIBILITY_COMPATIBLE;
            }
            else {
                return DP_PROTOCOL_COMPATIBILITY_MINOR_INCOMPATIBILITY;
            }
        }
    }
    return DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE;
}

bool DP_protocol_version_equals(const DP_ProtocolVersion *a,
                                const DP_ProtocolVersion *b)
{
    if (a == b) {
        return true;
    }
    else if (!a || !b) {
        return false;
    }
    else {
        return DP_str_equal(a->ns, b->ns) && a->server == b->server
            && a->major == b->major && a->minor == b->minor;
    }
}

bool DP_protocol_version_greater_or_equal(const DP_ProtocolVersion *a,
                                          const DP_ProtocolVersion *b)
{
    if (a == b) {
        return true;
    }
    else if (!a || !b) {
        return false;
    }
    else {
        return DP_str_equal(a->ns, b->ns)
            && (a->server > b->server
                || (a->server == b->server
                    && (a->major > b->major
                        || (a->major == b->major && a->minor >= b->minor))));
    }
}

uint64_t DP_protocol_version_as_integer(const DP_ProtocolVersion *protover)
{
    if (protover) {
        uint64_t max = ((uint64_t)1 << (uint64_t)21) - (uint64_t)1;
        uint64_t a =
            DP_clamp_uint64(DP_int_to_uint64(protover->server), 0, max);
        uint64_t b = DP_clamp_uint64(DP_int_to_uint64(protover->major), 0, max);
        uint64_t c = DP_clamp_uint64(DP_int_to_uint64(protover->minor), 0, max);
        return (a << (uint64_t)42) | (b << (uint64_t)21) | c;
    }
    else {
        return 0;
    }
}
