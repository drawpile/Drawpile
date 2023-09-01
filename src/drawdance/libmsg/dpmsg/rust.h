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

struct DP_ProtocolVersion *DP_protocol_version_parse(const char *s);

void DP_protocol_version_free(struct DP_ProtocolVersion *protover);

const char *DP_protocol_version_ns(struct DP_ProtocolVersion *protover);

int DP_protocol_version_server(struct DP_ProtocolVersion *protover);

int DP_protocol_version_major(struct DP_ProtocolVersion *protover);

int DP_protocol_version_minor(struct DP_ProtocolVersion *protover);

const char *DP_protocol_version_name(struct DP_ProtocolVersion *protover);

enum DP_ProtocolCompatibility
DP_protocol_version_client_compatibility(struct DP_ProtocolVersion *protover);

#endif /* DPMSG_RUST_H */
