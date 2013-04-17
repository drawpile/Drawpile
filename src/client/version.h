#ifndef VERSION_H
#define VERSION_H

#include "config.h"

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
 * level (e.g. starting from 128) unless they are fully compatible with
 * the official client.
 */
static const int level = 1; 

//! Version string
static const char string[] = DRAWPILE_VERSION;

}

#endif

