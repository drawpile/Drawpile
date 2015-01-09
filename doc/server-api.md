New REST API
------------

The standalone server provides a RESTful JSON API for administration.
The API is activated with the `--web-admin-port` command line parameter.
For security, it is important that the API is not publicly accessible!
If public access is needed (e.g. to list available sessions,) create a
web application that communicates with the server and presents the data.

Note. All paths have a common prefix, which is `/api` by default.
The body of POST and PUT messages should be a JSON document.

## Getting server status

`GET /status/`

Returns:

    {
        "title": server title,
        "activeSessions": number of active sessions (hibernated session do not count),
        "totalSessions": number of sessions in total, including hibernating ones,
        "maxActiveSessions": maximum number of active sessions,
        "totalUsers": total number of connected users,
        "flags": ["flag1", ...]
    }

Possible server flags are:

* `hostPass`: a password is needed to host a new session
* `persistent`: persistent sessions are enabled
* `secure`: Clients must upgrade to a secure connection before they can log in
* `hibernation`: session hibernation is enabled

## Changing server settings

`PUT /status/`

The following fields can be used in the request:

    {
        "title": new server title,
        "maxSessions": new session limit (will not affect existing sessions),
    }

Only the provided files will be changed.
The server will respond with updated status report.

## Getting a list of sessions

`GET /sessions/`

Returns:

        [
        {
                "id": session id,
                "title": session title,
                "userCount": current user count,
                "maxUsers": maximum user count (always 0 for hibernated sessions),
                "protoVersion": protocol minor version for this session,
                "flags": ["flag1", ...],
                "startTime": time in ISO8601 format
        }, ...
        ]

Session flags are:

* `closed`: no new users are admitted
* `persistent`: session will not be deleted when the last user leaves
* `asleep`: the session is hibernating
* `pass`: a password is needed to join

## Getting session details

`GET /sessions/<id>/`

Returns:

    {
        *same properties as in session list*,
        "historySize": session history size in megabytes (approximate),
        "historyLimit": session history limit (after this is exceeded, a new snapshot point will be generated),
        "historyStart": index of first element in history
        "historyEnd": index of last element in history
        "users": [
            {
                "ctxid": user's context id,
                "ip": user's IP address,
                "name": username,
                "flags": ["flag1", ...]
            }, ...
        ]
    }

Note. The user's context ID is unique only within the session and may be reused after the user logs out.

Possible user flags are:

* `op`: this user is a session operator (note: there is always at least one operator)
* `mod`: this user is a moderator (implies `op`)
* `locked`: this user has been locked
* `secure`: SSL in use
* `auth`: user has authenticated with a password

## Shutting down a session

`DELETE /sessions/<id>/`

Making this call will delete the session, even if it has been hibernated.

Responds with `{ success: true }`

## Kicking users

`DELETE /sessions/<session id>/user/<context id>/`

Nonpersistent sessions will automatically shut down when the last user is kicked.

Responds with `{ success: true }`

## Sending messages

`POST /wall/`

Request body:

    {
        "message": message content
    }

This request will send a system chat message to all logged in users. The message
can be targeted to the users of a specific session by using the session specific URL:
`/sessions/<id>/wall`

Responds with `{ success: true }`

