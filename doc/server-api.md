Drawpile server REST API
------------------------

The standalone server provides a JSON API for administration.

## Get server status

`GET /api/status/`

Returns:
    {
        "title": server title,
        "sessionCount": number of active sessions (hibernated session do not count),
        "maxSessions": maximum number of active sessions,
        "totalUsers": total number of connected users,
        "flags": ["flag1", ...]
    }

Possible server flags are:

* hostPass: a password is needed to host a new session
* persistent: persistent sessions are enabled
* secure: All clients must use SSL to log in
* hibernation: session hibernation is enabled

## Change server settings

`POST /api/status`

The following POST parameters can be set:

* "title": change server title

Server will respond with `{success: true}` if update was successfull.

## Get session list

`GET /api/sessions/`

Returns:

	[
	{
		"id": session id,
		"title": session title,
		"userCount": current user count,
		"maxUsers": maximum user count, (always 0 for hibernated sessions)
		"protoVersion": protocol minor version for this session,
		"flags": ["flag1", ...],
		"startTime": time in ISO8601 format
	}, ...
	]

Session flags are:

* closed: the session is closed for new users
* persistent: this is a persistent session
* asleep: the session is hibernating
* pass: password is needed to join

## Get session details

`GET /api/sessions/<id>`

Returns:

	{
		*same properties as in session list*,
		"historySize": session history size in megabytes (approximate),
		"historyLimit": session history limit (after this is exceeded, a new snapshot point will be generated),
		"historyStart": index of first element in history
		"historyEnd": index of last element in history
		"users": [
		{
			"id": user id,
			"name": user name,
			"flags": ["flag1", ...]
		}
	}

Possible user flags are:

* op: this user is a session operator (note: there is always at least one operator)
* locked: this user has been locked
* secure: this user connected using using SSL

## Shut down a session

`DELETE /api/sessions/<id>`

Making this call will delete the session, even if it has been hibernated.

