#ifndef DP_PROTO_CONSTANTS_H
#define DP_PROTO_CONSTANTS_H

namespace protocol {

/**
 * The magic number to identify the protocol
 */
static const char MAGIC[] = {'D', 'r', 'P', 'l'};

/**
 * The protocol revision. Servers should drop connections from
 * clients with a different protocol revision.
 */
static const int REVISION = 1;

/**
 * The default port to use.
 */
static const unsigned short DEFAULT_PORT = 27750;

}

#endif

