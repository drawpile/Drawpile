Server admin REST API
---------------------

The standalone server provides a RESTful JSON API for administration.
The API is activated with the `--web-admin-port` command line parameter.
For security, it is important that the API is not publicly accessible!
If public access is needed (e.g. to list available sessions,) create a
web application that communicates with the server and presents the data.

The body of POST and PUT messages should be a JSON document with the content type `application/json`.


## Serverwide settings

`GET /server/`

Returns a list of server settings:

    {
        "clientTimeout": "client connection timeout (e.g. 60s)",
        "sessionSizeLimit": "maximum session size (e.g. 5MB)",
        "sessionCountLimit": maximum number of active sessions,
        "enablePersistence": true/false (enable persistent sessions),
        "archiveMode": true/false (archive file backed sessions instead of deleting them),
        "idleTimeLimit": "delete session after it has idled for this long (e.g. 1h, set to 0 to disable)",
        "serverTitle": "title to be shown in the login box",
        "welcomeMessage": "welcome chat message sent to new users",
        "announceWhiteList": true/false (use announcement server whitelist),
        "privateUserList": true/false (if true, user list is never included in announcements),
        "allowGuestHosts": true/false (allow users without the HOST privilege to host sessions),
        "allowGuests": true/false (allow unauthenticated logins),
    }

To change any of these settings, send a `PUT` request. Settings not
included in the request are not changed.

Implementation: `serverJsonApi @ src/server/multiserver.cpp`


## Sessions

Get a list of active sessions: `GET /sessions/`

Returns:

    [
        {
            "id": "uuid (unique session ID)",
            "alias": "ID alias (if set)",
            "protocol": "session protocol version",
            "userCount": "number of users",
            "founder": "name of the user who created the session",
            "title": "session title",
            "hasPassword": true/false (is the session password protected),
            "closed": true/false (is the session closed to new users),
            "persistent": true/false (is persistence enabled for this session),
            "nsfm": true/false (does this session contain NSFM content),
            "startTime": "timestamp",
            "size": history size in bytes,
        }, ...
    ]

Implementation: `callSessionJsonApi @ src/shared/server/sessionserver.cpp`

Get detailed information about a session: `GET /sessions/:id/`

    {
        *same fields as above
        "maxSize": maximum allowed size of the session,
        "users": [
            {
                "id": user ID (unique only within the session),
                "name": "user name",
                "ip": "IP address",
                "auth": true/false (is authenticated),
                "op": true/false (is session owner),
                "muted": true/false (is blocked from chat),
                "mod": true/false (is a moderator),
                "tls": true/false (is using a secure connection)
            }, ...
        ],
        "listing": [
            {
                "id": listing entry ID number,
                "url": "listing server URL",
                "roomcode": "room code",
                "private": true/false
            }, ...
        ]
    }

Updating session properties: `PUT /session/:id/`

The following properties can be changed:

    {
        "closed": true/false,
        "persistent": true/false (only when persistence is enabled serverwide),
        "title": "new title",
        "maxUserCount": user count (does not affect existing users),
        "password": "new password",
        "opword": "new operator password",
        "preserveChat": true/false (include chat in history),
        "nsfm": true/false
    }

To send a message to all session participants: `PUT /session/:id/`

    {
        "message": "send a message to all participants",
        "alert": "send an alert to all participants"
    }

To shut down a session: `DELETE /sessions/:id/`

Implementation: `callJsonApi @ src/shared/server/session.cpp`

### Session users

Kick a user from a session: `DELETE /session/:sessionid/:userId/`

Change user properties: `PUT /session/:sessionid/:userId/`

    {
        "op": true/false (op/deop the user)
    }

To send a message to an individual user: `PUT /session/:sessionid/:userId/`

    {
        "message": "message text"
    }

Implementation: `callUserJsonApi @ src/shared/server/session.cpp`

## Logged in users

`GET /users/`

Returns a list of logged in users:

    [
        {
            "session": "session ID (if empty, this user hasn't joined any session yet)",
            "id": user ID (unique only within the session),
            "name": "user name",
            "ip": "IP address",
            "auth": true/false (is authenticated),
            "op": true/false (is session owner),
            "muted": true/false (is blocked from chat),
            "mod": true/false (is a moderator),
            "tls": true/false (is using a secure connection)
        }
    ]

Implementation: `callUserJsonApi @ src/shared/server/sessionserver.cpp`

## User accounts

`GET /accounts/`

Returns a list of registered user accounts:

    [
        {
            "id": internal account ID number,
            "username": "account name",
            "locked": true/false (is this account locked),
            "flags": "comma separated list of flags"
        }, ...
    ]

Possible user flags are:

 * HOST - this user can host new sessions (useful when allowGuestHosts is set to false)
 * MOD - this user is a moderator (has permanent OP status, may enter locked sessions)

To add a new user: `POST /accounts/`

    {
        "username": "username to register",
        "password": "user's password",
        "locked": true/false,
        "flags": ""
    }

To edit an user: `PUT /accounts/:id/`

    {
        "username": "change username",
        "password": "change password",
        "locked": change lock status,
        "flags": "change flags"
    }

To delete a user: `DELETE /accounts/:id/`

Implementation: `accountsJsonApi @ src/server/multiserver.cpp`

## Banlist

`GET /banlist/`

Returns a list of serverwide IP bans:

    [
        {
            "id": ban entry ID number,
            "ip": "banned IP address",
            "subnet": "subnet mask (0 means no mask, just the individual address is banned)",
            "expires": "ban expiration date",
            "comment": "Freeform comment about the ban",
            "added": "date when the ban was added"
        }
    ]

To add a ban, make a `POST` request to `/banlist/`:

    {
        "ip": "IP to ban",
        "subnet: "Subnet mask (0 to ban just the single address)",
        "expires": "expiration date",
        "comment": "freeform comment"
    }

To delete a ban, make a `DELETE /banlist/:id/` request.

Implementation: `banlistJsonApi @ src/server/multiserver.cpp`

## Server log

`GET /log/`

The following query parameters can be used to filter the result set:

 * ?page=0/1/2/...: show this page
 * ?session=id: show messages related to this session
 * ?after=timestamp: show messages after this timestamp

Returns:

    [
        {
            "timestamp": "log entry timestamp (UTC+0)",
            "level": "log level: Error/Warn/Info/Debug",
            "topic": "what this entry is about",
            "session": "session ID (if related to a session)",
            "user": "ID;IP;Username triple (if related to a user)",
            "message": "log message"
        }, ...
    ]

Possible topics are:

 * Join: user join event
 * Leave: user leave event
 * Kick: this user was kicked from the session
 * Ban: this user was banned from the session
 * Unban: this user's ban was lifted
 * Op: this user was granted session ownership
 * Deop: this user's session owner status was removed
 * Mute: this user was blocked from chat
 * Unmute: this user can chat again
 * BadData: this user sent an invalid message
 * RuleBreak: this committed a protocol violation
 * PubList: session announcement related messages
 * Status: general status messages

Implementation: `logJsonApi @ src/server/multiserver.cpp`
