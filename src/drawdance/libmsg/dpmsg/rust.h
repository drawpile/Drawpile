// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DPMSG_RUST_H
#define DPMSG_RUST_H

// This file is auto-generated , don't edit it manually.
// To regenerate it, run src/drawdance/generators/cbindgen.bash

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum DP_ProtocolCompatibility {
    DP_PROTOCOL_COMPATIBILITY_COMPATIBLE,
    DP_PROTOCOL_COMPATIBILITY_MINOR_INCOMPATIBILITY,
    DP_PROTOCOL_COMPATIBILITY_BACKWARD_COMPATIBLE,
    DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE,
} DP_ProtocolCompatibility;

typedef struct DP_ProtocolVersion DP_ProtocolVersion;

struct DP_ProtocolVersion *DP_protocol_version_new(const char *ns, int server,
                                                   int major, int minor);

struct DP_ProtocolVersion *DP_protocol_version_new_major_minor(int major,
                                                               int minor);

struct DP_ProtocolVersion *DP_protocol_version_new_current(void);

struct DP_ProtocolVersion *
DP_protocol_version_new_clone(const struct DP_ProtocolVersion *protover);

struct DP_ProtocolVersion *DP_protocol_version_parse(const char *s);

void DP_protocol_version_free(struct DP_ProtocolVersion *protover);

bool DP_protocol_version_valid(const struct DP_ProtocolVersion *protover);

bool DP_protocol_version_is_current(const struct DP_ProtocolVersion *protover);

bool DP_protocol_version_is_future(const struct DP_ProtocolVersion *protover);

bool DP_protocol_version_is_past_compatible(
    const struct DP_ProtocolVersion *protover);

const char *DP_protocol_version_ns(const struct DP_ProtocolVersion *protover);

int DP_protocol_version_server(const struct DP_ProtocolVersion *protover);

int DP_protocol_version_major(const struct DP_ProtocolVersion *protover);

int DP_protocol_version_minor(const struct DP_ProtocolVersion *protover);

const char *DP_protocol_version_name(const struct DP_ProtocolVersion *protover);

enum DP_ProtocolCompatibility DP_protocol_version_client_compatibility(
    const struct DP_ProtocolVersion *protover);

bool DP_protocol_version_equals(const struct DP_ProtocolVersion *a,
                                const struct DP_ProtocolVersion *b);

uint64_t
DP_protocol_version_as_integer(const struct DP_ProtocolVersion *protover);

#endif /* DPMSG_RUST_H */
