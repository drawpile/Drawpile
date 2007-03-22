#ifndef VERSION_H
#define VERSION_H

namespace version {

//! Protocol level
/**
 * A client revision level. Sometimes clients change their interpretation
 * of the protocol, but still remain compatible with the server. The
 * revision level is used to prevent incompatable clients from joining
 * the same sessions.
 *
 * The revision level is usually reset to 0 after protocol compatability
 * is broken. Other implementations of DrawPile should use a different
 * level (e.g. starting from 256) unless they are fully compatible with
 * the official client.
 */
static const int level = 0; 

//! Version string
static const char *string = "0.4.0";

}

#endif

