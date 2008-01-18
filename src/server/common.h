#pragma once

#ifndef ServerCommons_GUARD
#define ServerCommons_GUARD

#include "config.h"

#include "shared_ptr.h"
#include "../shared/protocol.fwd.h"

typedef SharedPtr<protocol::Message> message_ref;

class Socket;
class Address;

class Server;
class User;

class Session;
class SessionData;

class LayerData;

class Buffer;

struct Statistics;

#endif // ServerCommons_GUARD
