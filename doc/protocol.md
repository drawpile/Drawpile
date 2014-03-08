Drawpile protocol
-----------------

Drawpile uses a message based protocol. Each message begins with a header containing the length of the message and the type. Messages can be divided into three categories: login, meta and command.

The login category contains just one message type: the login message. It is used during the login handshake.

The most important category is the command category. Command messages are used to perform drawing and other canvas manipulation. The bulk of Drawpile traffic consists of command messages.

Last, meta messages deal with things that do not directly affect drawing, such as user login notifications, access control adjustments and chat messages. Meta messages are not strictly needed for drawing: even if they are all filtered out, drawing should still work as usual.

Each message begins with a header indicating its length and type:

    ┌────────┬──────┬────────────┐
    │ LENGTH │ TYPE │ PAYLOAD... │
    └────────┴──────┴────────────┘

The length field indicates the length of the payload data. The full length of the message is 3 + payload length. Message type 0 is the login message. Types <128 are meta messages and >=128 are command messages.

See `src/shared/net/message.h` for the full list of message types.

A subset of the network protocol is used as the session recording format.

## Protocol revision history

The protocol version number consists of two parts: the major and the minor number. Change in the major number indicates changes that break compatibility between the server and the client. Change in the minor number indicate smaller changes in the client's interpretation of the drawing commands.

Clients can connect to any server sharing the same major protocol version number, but all clients in the same session must share the exact version. Version numbers are also used to determine whether a session recording is compatible with the user's client version.

Protocol 8.2:

 * MovePointer command coordinates now use 1/4 pixel resolution (same as normal brushes)
 * Recordings are fully backward compatible with 8.1

Protocol 8.1:

 * New command: FillRect
 * New meta command: MovePointer
 * Fully backward compatible with 7.1

Protocol 7.1:

 * First protocol revision that can be recorded.

