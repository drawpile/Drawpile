Drawpile protocol
-----------------

Drawpile uses a message based protocol. Each message begins with a header containing the length of the message and the type. Messages can be divided into three categories: login, meta and command.

The login category contains just one message type. It is used only during the login handshake.

The most important category is the command category. Command messages are used to perform drawing and other canvas manipulation. The bulk of Drawpile traffic consists of command messages.

Last, meta messages deal with things that do not directly affect drawing, such as user login notifications, access control adjustments and chat messages. Meta messages are not strictly needed for drawing: even if they are all filtered out, drawing should still work as usual.

Each message begins with a header indicating its length and type:

    ┌────────┬──────┬────────────┐
    │ LENGTH │ TYPE │ PAYLOAD... │
    └────────┴──────┴────────────┘

The length field indicates the length of the payload data. The full length of the message is 3 + payload length. Message type 0 is the login message. Types <128 are meta messages and ≥128 are command messages.

See `src/shared/net/message.h` for the full list of message types.

A subset of the network protocol is used as the session recording format.

## Login process

The login handshake is a string based protocol that uses the LOGIN message type. The handshake begins when the user connects to the server port.

The server starts by sending a hello message, which includes the protocol version and the set of server feature flags. If the client does not support the protocol or does not understand the features, it should disconnect.

If the SECURE feature flag is set, the server will not let the login process continue until the client has upgraded to a secure connection.

Next, the client must authenticate. The client will first attempt a guest login, but if that is not possible, it will prompt the user for a password and retry. An authenticated user may be granted extra privileges by the server.

After authentication, the server continues will send a list of available sessions. The client can either join one of the sessions or host a new one. The server may send updated session information until the client has made a decision.

The built-in server only supports a single session per server instance. This is indicated by (the lack of) a feature flag. In this case, the client may join the only session automatically without prompting the user.

The server responds to the join/host command either by a success signal or an error code. If the command was accepted, the client leaves the login state and enters the session. In case of error, the server disconnects the client. Users joining a session will be assigned a user ID by the server. Hosting users can choose their own IDs.

When hosting a session, the hosting user must announce its minor protocol version. A joining user must not join a session with a mismatching minor version. This way, a single server can support multiple client versions as long as the major versions match.

See `src/shared/server/loginhandler.h` for implementation details.

## Security

The server supports SSL/TLS encrypted connections. The client upgrades to a secure connection by sending the command "STARTTLS". The server will reply with a STARTTLS of its own and begins the SSL handshake.

Since most Drawpile users will likely run drawpile-srv on their home computers or small hosting services, Drawpile does not utilize PKI. The client accepts self-signed certificates and, when connecting to an IP address, certificates that do not match the hostname of the server. Instead, the client will remember the certificate associated with each hostname and warns if it changes.

## Session recording format

A session recording starts with a header that identifies the file type, followed by messages in the same format as transmitted over the network. All command messages and a select few meta message types are recorded.

There are two meta message types that are relevant only to recordings:

* Interval: this message is used to save timing information. Upon encountering an interval message, the client should pause for the given number of milliseconds.
* Marker: these are markers created by the user to annotate interesting points in the session. They can be used like bookmarks during playback.

The recording header can be described as follows:

    struct Header {
        char magic[7];        // "DPRECR\0"
        uint32_t version;     // Protocol version number (major and minor)
        char clientversion[]; // Client version string (variable length, \0 terminated)
    };

The recording can be accompanied by an *index file*. The index file must have the same name as the recording file, except for the file extension, which must be "dpidx".

The format of the index file is not documented and it can change from version to version. However, the index can always be regenerated from the main recording. The index contains a map of message offsets and snapshots of drawingboard state to enable quick jumping in the recording.

A session hibernation file is a special type of recording with an extended header. Also, all messages types are recorded in the hibernation file.

    struct HibernationHeader {
        char magic[7];         // "DPRECH\0"
        uint32_t version;      // Protocol version number (server major and session minor)
        char serverversion[];  // Server version string (variable length, \0 terminated)
        uint8_t hibformat;     // Hibernation format version
    
        uint16_t titlelen;     // Session title length
        char title[titlelen];  // Session title
    
        uint16_t fnamelen;     // Session founder name length
        char fname[fnamelen];  // Session founder name
    
        uint8_t flags;         // Session flags bit field
    
        uint16_t passwdlen;    // Session password length
        char fname[passwdlen]; // Session password (algorith;hash)
    };


## Protocol revision history

The protocol version number consists of two parts: the major and the minor number. Change in the major number indicates changes that break compatibility between the server and the client. Change in the minor number indicate smaller changes in the client's interpretation of the drawing commands.

Clients can connect to any server sharing the same major protocol version number, but all clients in the same session must share the exact version. Version numbers are also used to determine whether a session recording is compatible with the user's client version.

Protocol 12.4 (0.9.6)

 * Added Ping message type
 * Added smudge fields to ToolChange message (reserved for future use)
 * Brush size now means brush diameter instead of radius
 * Login protocol change: session ID prefixed with '!' means ID was user specified

Protocol 11.3 (0.9.5)

 * Added preserve alpha blending mode
 * Fully backwards compatible

Protocol 11.2 (0.9.3)

 * Added ACTION flag to Chat message.
 * Fully backwards compatible: No server changes needed and older clients will simply ignore the flag.

Protocol 11.2 (0.9.2)

 * Various changes to the login process to fix bugs and to support authenticated logins
 * Added maxUsers field and PERSISTENT flag to SessionConf message
 * Increased SessionConf::attrs length to 16 bits to make room for more future attributes
 * Session ID format change: replaced numeric IDs with strings of format [a-zA-Z0-9:-]{1,64}
 * Added "flags" field to Chat message
 * Changed some operator commands
 * Increased UserAttr::attrs length to 16 bits to make room for more future attributes
 * Added MOD and AUTH UserAttr attributes
 * Use int32 coordinates in PutImage, CanvasResize and FillRect
 * Breaks compatiblity with previous version
 * First version to support session hibernation

Protocol 10.2 (0.9.0)

 * New login process to support multiple sessions on a single server
 * Added Disconnect message
 * No change in recording format

Protocol 9.2: (0.8.6)

 * MovePointer command coordinates now use 1/4 pixel resolution (same as normal brushes)
 * MovePointers were not recorded in earlier versions, so backwards compatibility is maintained.
 * New meta command: Marker
 * Client side change: minor changes to stroke rendering
 * Recordings are backward compatible with 7.1 and 8.1, but strokes may be drawn slightly differently

Protocol 8.1:

 * New command: FillRect
 * New meta command: MovePointer
 * Fully backward compatible with 7.1

Protocol 7.1:

 * First protocol revision that can be recorded.

