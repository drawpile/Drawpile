Drawpile protocol messages
--------------------------

This document describes the messages used in the Drawpile protocol.
The same message format is used both in the network protocol and in recording
files. For a more general overview of the protocol, refer to `protocol.md`.

## Type conventions

    uintX   X bit unsigned integer (big endian)
    intX    X bit signed integer (big endian)
    bitsX   X bit unsigned integer used as a bit field
    utf8*   variable length byte string containing UTF-8 encoded text
    bin*    variable length byte string containing binary data

Length of a variable string is the message payload length minus
length of the fixed parts. If a message contains more than one variable
length part, additional fields are included to indicate the lengths.

Unused bits in bitfields are reserved for future expansion.

## Message structure

The messages are divided into three categories: control, meta and command.

Control messages are used for server-client communication, such as the login
handshake and updating session setting.

Meta messages relay information not directly related to drawing, such as user login
notifications, chat and access control properties. Meta messages do not directly
affect the content of the canvas, but can affect the filtering of future messages,
including command messages.

Command messages are the actual drawing commands. All non-command messages can be
filtered away from a recording and the resulting image should be pixel by pixel the
same as before.

Additionally, messages can also be divided into two types: transparent and opaque.
All control messages and some of the meta messages are transparent, meaning the server
is aware of their semantics. (E.g. the server is responsible for notifying clients about
new users by sending UserJoin messages.)

Opaque messages are messages whose content the server does not need to understand, merely
add to the session history and relay to the clients. This division allows new client versions
to add new commands while still retaining compatibility with older server, or new servers to
retain compatibility with old clients. Examples of opaque messages include the "MovePointer" meta
message and all command messages.

Each message is preceded by a header indicating its type and length.
The format of the message envelope is:

    uint16 payload length
    uint8  message type
    uint8  context (user) ID
    bin*   payload

## Messages

You can find the up-to-date list of supported messages in the file `src/libshared/net/message.h`

Explanation of each message type can be found in the header files in the `net` folder. The exact
layout of the message can be read from the source code in the cpp files. (See the `serialize` and
`deserialize` functions.)
