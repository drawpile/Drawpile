Authenticated logins
---------------------

Drawpile supports two types of logins: guest logins and authenticated logins.

A guest login is a typical login without a password. The built-in server supports only guest logins. When a password is provided, the login is considered authenticated. To enable auth logins, a user management backend must be enabled in the server. Currently, only a userfile based backend is implemented.

The API for the user management interface can be found in `src/shared/server/identitymanager.h`. In short, the identity manager is passed a username and password pair, and it returns a failure/success type response.

The username query should be case insensitive, but the identity manager may apply other normalizations to the name as well (e.g. convert spaces to underscores.)

Valid responses for the login attempt are:

* user not found: meaning this is not a managed username and may be used for guest login
* bad password: this username is managed and the provided password (if any) was wrong
* banned: this username has been banned
* ok: username and password OK, user is authenticated.

During the login phase, the client may first attempt to log in as a guest by omitting the password. If the identity manager replies with "user not found", the server will allow the guest login. If the response is "bad password", the client will prompt the user for a password and try again. A login attempt with an incorrect password is rejected.

The identity manager is allowed to always return "bad password" instead of "user not found" if it wishes to avoid leaking information about existing users. However, guest logins will not be possible in this case.

A successful login response from the identity manager contains two extra fields:

* Canonical username: the preferred form of the username. This is typically the username as stored in the database, but the manager may change the name in any way as long as it remains unique. (E.g. the server could enforce some naming scheme, such as prepending "guest-" to the name of all unauthenticated users.)
* User flags: extra privileges assigned to the user.

Supported user flags are:

 * mod: this user is a moderator. A moderator may log in to any (even closed and password protected) session and has a permanent OP status.
 * host: this user may host sessions without providing a hosting password.

## User file

The userfile backend reads the user list from a file. The file consists of lines in the following format:

    <username>:<password hash>:<flags>

The password hash begins with `algorithm;` to indicate the used hashing algorithm. How the rest of the hash string is interpreted depends on the algorithm. Currently only "salted SHA1" (`s+sha1`) is supported.

If the password hash token is prefixed with `*`, the username is considered banned.

To enable the userfile backend, pass drawpile-srv the parameter `--userfile <path>`. The server will monitor the file for changes and reload it when necessary.

A python script for editing user files is provided in `server/usertool.py`. 

