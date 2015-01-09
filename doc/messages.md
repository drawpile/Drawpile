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

Fields annotated with Ⓒ are *client-defined*, meaning their semantics
are irrelevant to the server. Since the interpretation of these fields
is tied heavily to the implementation of the client, they are not defined
in this document.

## Message structure

The messages are divided into three categories: login, meta and command.
Login messages are used only during the login phase. Meta messages relay
information not directly related to drawing (user login notifications, chat,
access controls, etc.) Command messages are used to perform the actual drawing.
All non-command messages can be filtered away from a recording and the resulting
image should be pixel by pixel the same as before.

Each message is preceded by a header indicating its type and length.
The format of the message envelope is:

    uint16 payload length
    uint8  message type
    bin*   payload

## Login messages

The login category contains just one message type: `MSG_LOGIN`.

### MSG_LOGIN (0)

    utf8* login message

The login message is very simple: it's entire payload is just a UTF-8 encoded
string. The login handshake is described in more detail in `login.md`.

## Meta messages

### MSG_USER_JOIN (1)

    uint8 user ID
    utf8* username

This message is sent by the server to inform the client of a new user. Note
that since this message is a *meta* message, the client should NOT rely on
this message to be sent before drawing commands from the same ID are sent.
In other words, this message does not initialize a new drawing context,
but merely associates a username with the ID.

### MSG_USER_ATTR (2)

    uint8  user ID
    bits16 attributes
        0x01 - user is locked
        0x02 - user is a session operator
        0x04 - user is a moderator
        0x08 - user is authenticated (i.e. not a guest)

This message is sent by the server to notify of changes to user attributes.
The client can show this information on the user interface, but it must not
be used to change the interpretation of *command* messages. Even if a user is
locked, drawing commands received from the server must be applied. (I.e. the
server is in charge of enforcing access controls.)
The client may, however, lock the user interface when the local user is locked.

### MSG_USER_LEAVE (3)

    uint8 user ID

This message is sent to indicate a user has left the session. Bending the
rules a little, it is acceptable for a client to destroy the related
drawing context when receiving this message, as (for a well behaved server)
this message is a guarantee that no further drawing commands will be sent
for this user.

### MSG_CHAT (4)

    uint8 user ID
    bits8 flags
        0x01 - this is an announcement
        0x02 - operator command
        0x04 - "action" message
    utf8* message

Chat messages are mostly self explanatory, but there are a few special cases:

 * Messages sent from the reserved user ID 0 are server notifications
 * Messages bypass the session history and are delivered directly to currently logged in users (except when the announcement bit is set or "save chat" session flag is set)
 * Clients may style announcements differently from normal messages
 * Operator commands are IRC style commands that affect session settings.
 * Action messages are like normal chat messages, but are rendered differently. They are used like the "/me" IRC command.

### MSG_LAYER_ACL (5)

    uint8 user ID
    uint16 layer ID
    uint8 locked (boolean)
    bin8* exclusive IDs

This message is used to change layer access controls . The user ID is the
ID of the user who issued the command. Note that as this is a meta command,
the client should only use this information to update the UI and not filter
the received commands. Only session operators may send this command.

The exclusive ID string is a list of users who get exclusive access to the
layer. When not zero-length, the layer is considered locked for everyone else.

### MSG_SNAPSHOT (6)

    uint8 mode

This message is used when transmitting a snapshot of the current session state
to the server. This is done when the server does not have the whole session
history in memory, but needs it to allow new users to join. This happens
when the session is first created and later when a memory limit is reached
and session history is purged.

The mode field can be one of:

    0x00 - Request
    0x01 - Request new
    0x02 - ACK
    0x03 - Snapshot modifier
    0x04 - End

The server will initiate a snapshot request by sending a Snapshot message with
mode 0 or 1. The *request new* means the server would like a freshly generated
snapshot rather than what the client may already have in memory. (The idea is
that a fresh snapshot may be smaller than the whole history.)

Upon receiving the request, the client will create the snapshot and respond
with ACK to let the server know the snapshot is ready. After that, the client
will send the snapshot in parallel with normal traffic. Messages belonging
to the snapshot are identified by preceding them with Snapshot modifier mode
Snapshot message.
Snapshot 0x03 + 0x04 message pair signals the end of the snapshot.

### MSG_SNAPSHOTPOINT (7)

Internal use only. This message is never serialized.

### MSG_SESSION_TITLE (8)

    uint8 user ID
    utf8* title

Change the session title. The user must be a session operator to send this.
The session title can also be changed with a chat command.

### MSG_SESSION_CONFIG (9)

    uint8  max users
    bits16 flags
        0x01 - session locked
        0x02 - session closed (no new users can join)
        0x04 - layer controls locked (only operators can add/delete layers)
        0x08 - new users start out locked
        0x10 - persistent session (remains on server even after all users have left)
        0x20 - preserve chat (all chat messages are preserved in session history)

This message is sent by the server to inform clients of changes to session
settings. Chat operator commands are used to change these settings.

### MSG_STREAMPOS (10)

    uint32 bytes left

This message is sent by the server when there is a large amount of data queued
for transmission. Note that more data can be added to the queue after the
message has been sent, so this information should be used for anything more
precise than showing a progress bar to the user.

### MSG_INTERVAL (11)

    uint16 pause in milliseconds

This message indicates a pause. It is used to save timing information in
recordings and is not usually sent over the network.

### MSG_MOVEPOINTER (12)

    uint8 user ID
    int32 x Ⓒ
    int32 y Ⓒ
    uint8 persistence Ⓒ

Move user pointer marker. Normally the marker is moved automatically when
drawing, but it can also be moved explicitly with this message, allowing
the marker to be used to point out things on the canvas.
The *x* and *y* fields are encoded the same way as in `MSG_PEN_MOVE`.

When *persistence* is nonzero, a fading line using the current brush color
is drawn between the new pointer position and the old one. The line is an
overlay and not part of the actual drawing canvas. This is used to
implement a virtual laser pointer tool.

### MSG_MARKER (13)

    uint8 user ID
    utf8* label

A bookmark. This is label specific time points on a recording.

### MSG_DISCONNECT (14)

    uint8 reason
    utf8* message

Graceful disconnect notification. When disconnecting normally, this message
is sent to indicate the reason. Possible reasons are:

    0x00 - Error occurred
    0x01 - User was kicked
    0x02 - Program is shutting down
    0x03 - Other reason

When the user is kicked, the disconnect message is the username of the kicker.

### MSG_PING (15)

	uint8 pong

## Command messages

### MSG_CANVAS_RESIZE (128)

    uint8 user ID
    int32 top
    int32 right
    int32 bottom
    int32 left

This command is used to resize the drawing canvas. The values are relative to
the current size.

The session should start with this command to set the initial canvas size.

### MSG_LAYER_CREATE (129)

    uint8  user ID
    uint16 layer ID
    uint32 fill color Ⓒ
    utf8*  title

Create a new layer, filled with the given color (ARGB). The newly created layer
is placed at the top of the layer stack. If layer controls are locked (session
option,) only operators may use this command.

The layer ID should be set to zero when this command is sent by a client.
The server will assign an available ID to it.

### MSG_LAYER_COPY (130)

Reserved for future implementation.

### MSG_LAYER_ATTR (131)

    uint8 user ID
    uint16 layer ID
    uint8 opacity Ⓒ
    uint8 blend mode Ⓒ

Change layer attributes. If layer controls are locked, only operators can
use this command.

### MSG_LAYER_RETITLE (132)

    uint8 user ID
    uint16 layer ID
    utf8* new title

Change layer title. If layer controls are locked, only operators can
use this command.

### MSG_LAYER_ORDER (133)

    uint8  user ID
    uint8* layer IDs

Change layer orders.
This command includes a list of layer IDs that define the new stacking order.
An order change command sent by the server must list all layers, while
a command sent by the client may be missing (newly created) layers,
in which case the missing layers will be included at the top of the stack in
their existing relative order.

For example: if the current stack is `[1,2,3,4,5]` and the client sends
a reordering command `[3,2,1]`, the server must add the missing layers:
`[3,2,1,4,5]`.

If layer controls are locked, only operators can use this command.

### MSG_LAYER_DELETE (134)

    uint8 user ID
    uint16 layer ID
    uint8 merge (boolean)

Delete the given layer. If either the layer or layer controls are locked,
this command requires session operator privileges.

If the merge flag is set, the layer will be merged with the one below it.
The user must have write access to the layer below. The bottom-most layer
cannot be merged.

### MSG_PUTIMAGE (135)

    uint8  user ID
    uint8  layer ID
    bits8  flags Ⓒ
    uint32 x Ⓒ
    uint32 y Ⓒ
    uint32 w Ⓒ
    uint32 h Ⓒ
    bin*   image Ⓒ

Draw a bitmap onto a layer. If the target layer is locked, this command is
ignored.

### MSG_TOOLCHANGE (136)

    uint8 user ID
    uint16 layer ID
    uint8 blend mode Ⓒ
    uint8 dab spacing Ⓒ
    uint32 color-hi Ⓒ
    uint32 color-lo Ⓒ
    uint8  hardness-hi Ⓒ
    uint8  hardness-lo Ⓒ
    uint8  size-hi Ⓒ
    uint8  size-lo Ⓒ
    uint8  opacity-hi Ⓒ
    uint8  opacity-lo Ⓒ
    uint8  smudge-hi Ⓒ
    uint8  smudge-lo Ⓒ
    uint8  resmudge Ⓒ

Change drawing context properties. Most of the fields are client defined, but
the current layer ID is relevant to the server as well, since the server
must track the users' layer selections to enforce layer access controls.

A toolchange should be sent before the first `PEN_MOVE`.

### MSG_PEN_MOVE (137)

    uint8 user ID
    struct* {
        int32  x Ⓒ
        int32  y Ⓒ
        uint16 pressure Ⓒ
    }

Pen movement. The first pen move when the drawing context is in pen-up state
starts a new stroke. A single pen move message may contain multiple coordinate
sets. (The limit is ⌊2¹⁶-1 / (4+4+2)⌋ = 6553)

If no toolchange command has been sent for the user, the results are undefined.
If the target layer is locked, this command is ignored.

### MSG_PEN_UP (138)

    uint8 user ID

End active stroke. If the drawing context is already in pen-up state, this
command has no effect.

### MSG_ANNOTATION_CREATE (139)

    uint8  user ID
    uint8  annotation ID
    int32  x Ⓒ
    int32  y Ⓒ
    uint16 w Ⓒ
    uint16 h Ⓒ

Create a new annotation. As with layers, the client should set the ID to zero
and the server will assign an available ID.

### MSG_ANNOTATION_RESHAPE (140)

    uint8  user ID
    uint8  annotation ID
    int32  x Ⓒ
    int32  y Ⓒ
    uint16 w Ⓒ
    uint16 h Ⓒ

Change annotation position and size.

### MSG_ANNOTATION_EDIT (141)

    uint8  user ID
    uint8  annotation ID
    uint32 background color Ⓒ
    utf8*  contents Ⓒ

Change annotation contents.

### MSG_ANNOTATION_DELETE (142)

    uint8  user ID
    uint8  annotation ID

Delete an annotation

### MSG_UNDOPOINT (143)

    uint8  user ID

An undo demarcation point. This marks the start of an undoable sequence of commands.
This also marks all previously undone actions are non-redoable. (TODO add diagram
to illustrate why.)

### MSG_UNDO (144)

    uint8 user ID
    uint8 override user ID
    int8  number of actions to undo

Undo or redo the given number of undoable sequences. Session operators may set the
override ID to undo other users' actions.

An undo is performed as follows:

1. Let U be the target user (user who issued command or override user)
2. Let P be the nth undopoint belonging to U (counting backwards from the end
   of the session,) where n is the number of actions to be undone.
3. Roll session back to some point before P
4. Replay session, skipping all commands by U, starting at P and marking them as undone

TODO explain Redo

### MSG_FILLRECT (145)

    uint8  user ID
    uint16 layer ID
    uint8  blend mode Ⓒ
    uint32 x Ⓒ
    uint32 y Ⓒ
    uint32 w Ⓒ
    uint32 h Ⓒ
    uint32 color Ⓒ

Fill a rectangle with solid color. If the target layer is locked, this command is ignored.

