// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPMSG_PROTOVER_H
#define DPMSG_PROTOVER_H
#include <dpcommon/common.h>

typedef enum DP_ProtocolCompatibility {
    DP_PROTOCOL_COMPATIBILITY_COMPATIBLE,
    DP_PROTOCOL_COMPATIBILITY_MINOR_INCOMPATIBILITY,
    DP_PROTOCOL_COMPATIBILITY_BACKWARD_COMPATIBLE,
    DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE,
} DP_ProtocolCompatibility;

typedef struct DP_ProtocolVersion DP_ProtocolVersion;

DP_ProtocolVersion *DP_protocol_version_new(const char *ns, int server,
                                            int major, int minor);

DP_ProtocolVersion *DP_protocol_version_new_major_minor(int major, int minor);

DP_ProtocolVersion *DP_protocol_version_new_current(void);

DP_ProtocolVersion *
DP_protocol_version_new_clone(const DP_ProtocolVersion *protover);

DP_ProtocolVersion *DP_protocol_version_parse(const char *s);

void DP_protocol_version_free(DP_ProtocolVersion *protover);

// Returned string must be freed via DP_free.
char *DP_protocol_version_to_string(const DP_ProtocolVersion *protover);

bool DP_protocol_version_valid(const DP_ProtocolVersion *protover);

bool DP_protocol_version_is_current(const DP_ProtocolVersion *protover);

bool DP_protocol_version_is_future(const DP_ProtocolVersion *protover);

bool DP_protocol_version_is_past_compatible(const DP_ProtocolVersion *protover);

bool DP_protocol_version_should_have_system_id(
    const DP_ProtocolVersion *protover);

const char *DP_protocol_version_ns(const DP_ProtocolVersion *protover);

int DP_protocol_version_server(const DP_ProtocolVersion *protover);

int DP_protocol_version_major(const DP_ProtocolVersion *protover);

int DP_protocol_version_minor(const DP_ProtocolVersion *protover);

const char *DP_protocol_version_name(const DP_ProtocolVersion *protover);

DP_ProtocolCompatibility
DP_protocol_version_client_compatibility(const DP_ProtocolVersion *protover);

bool DP_protocol_version_equals(const DP_ProtocolVersion *a,
                                const DP_ProtocolVersion *b);

bool DP_protocol_version_greater_or_equal(const DP_ProtocolVersion *a,
                                          const DP_ProtocolVersion *b);

uint64_t DP_protocol_version_as_integer(const DP_ProtocolVersion *protover);

#endif
