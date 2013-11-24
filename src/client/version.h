#ifndef VERSION_H
#define VERSION_H

#include "config.h"

namespace version {

/**
 * @brief protocol minor revision number.
 *
 * A server can support any client sharing the major protocol
 * revision, but clients may not interoperate if their minor
 * numbers do not match. The server enforces none of this though,
 * it is up to the client to abandon the login attempt in case of version
 * mismatch.
 *
 * The session's minor revision number is set by the hosting user.
 * The smallest revision number is 1. Zero revision is used to indicate
 * that the number has not been set yet.
 */
static const int MINOR_REVISION = 1;

}

#endif

