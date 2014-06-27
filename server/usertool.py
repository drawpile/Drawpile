#!/usr/bin/python3
"""
Drawpile server user file management tool.

Usage:

Adding new users:
    usertool.py add <filename> <username> <password> [--flags <flag1,flag2,...>]

Changing existing users:
    usertool.py update <filename> <username> [--password <new password<] [--flags <new flags>]

Listing user file content:
    usertool.py show <filename>

Supported user flags are:

 * mod  - this user is a moderator. A moderator can log in to any running
          session and has permanent OP privileges.
 * host - user can host sessions without the host password

Using the asterisk '*' as a user password will mark that username as
reserved, preventing its use.
If a key derivation function is not explicitly defined, the best one
available will be used.
"""

from collections import namedtuple
from binascii import hexlify

import hashlib
import argparse
import sys
import os

class PasswdFileError(Exception):
    pass

class UserLine(namedtuple('UserLine', ('user', 'password', 'flags'))):
    def __str__(self):
        return '{0}:{1}:{2}'.format(self.user, self.password, ','.join(self.flags))

def _tohex(bytestr):
    return hexlify(bytestr).decode('ascii')

def read_password_file(passwdfile):
    with open(passwdfile, 'r') as f:
        linen = 0
        users = []
        for line in f:
            linen += 1
            line = line.strip()
            if not line or line[0] == '#':
                continue

            try:
                user, passwd, flags = line.split(':')
            except TypeError:
                raise PasswdFileError("Error on line {0}: {1}".format(linen, line))

            users.append(UserLine(user, passwd, flags.split(',')))

        return users

def hash_password(password, keyfunc=None):
    # Special case: '*' in place of a password indicates a disabled username
    if password == '*':
        return password

    if keyfunc is None or keyfunc == 's+sha1':
        return _salted_sha1_hash(password)

    raise PasswdFileError("Unsupported key derivation function: " + keyfunc)

def _salted_sha1_hash(password):
    salt = os.urandom(16)
    m = hashlib.sha1()
    m.update(salt)
    m.update(password.encode('utf-8'))
    return 's+sha1;{0};{1}'.format(_tohex(salt), _tohex(m.digest()))

def _parse_flags(flagstr):
    """Parse and validate user flag list"""
    if not flagstr:
        return []

    flags = tuple(filter(bool, (f.strip().lower() for f in flagstr.split(','))))
    for f in flags:
        if f not in ('mod', 'host'):
            raise PasswdFileError("Unknown user flag: " + f)
    return flags

def add_user(passwdfile, user, passwd, flags='', keyfunc=None, nocheck=False):
    if not nocheck:
        try:
            existing = set(u.user.lower() for u in read_password_file(passwdfile))
            if user in existing:
                raise PasswdFileError("User {0} already exists!".format(user))
        except FileNotFoundError:
            pass

    with open(passwdfile, 'a') as f:
        f.write(str(UserLine(user, hash_password(passwd, keyfunc), _parse_flags(flags))))
        f.write('\n')

def update_user(passwdfile, user, passwd=None, flags=None, keyfunc=None):
    users = read_password_file(passwdfile)
    lname = user.lower()
    for idx, user in enumerate(users):
        if user.user.lower() == lname:
            break

    else:
        raise PasswdFileError("No such user: " + user)

    newpass = user.password
    newflags = user.flags

    if passwd is not None:
        newpass = hash_password(passwd, keyfunc)

    if flags is not None:
        newflags = _parse_flags(flags)

    users[idx] = UserLine(user.user, newpass, newflags)

    with open(passwdfile, 'w') as f:
        for u in users:
            f.write(str(u))
            f.write('\n')

def show_file(passwdfile):
    users = read_password_file(passwdfile)

    namelen = 0
    hashlen = 0
    for u in users:
        namelen = max(namelen, len(u.user))
        try:
            if u.password.startswith("*"):
                hashlen = max(hashlen, len("banned"))
            else:
                hashlen = max(hashlen, u.password.index(';'))
        except ValueError:
            hashlen = max(hashlen, len(u.password)+2)

    for u in users:
        try:
            if u.password.startswith("*"):
                hashfunc = "banned"
            else:
                hashfunc = u.password[:u.password.index(';')]
        except ValueError:
            hashfunc = '?' + u.password + '?'

        print("{name}{namepad} {hash}{hashpad} [{flags}]".format(
            name=u.user,
            namepad=" "*(namelen-len(u.user)),
            hash=hashfunc,
            hashpad=" "*(hashlen-len(hashfunc)),
            flags=', '.join(u.flags)
        ))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog="usertool.py",
        description="Drawpile server user file management tool"
        )

    # Common arguments
    subcommands = parser.add_subparsers(help="sub-command help")

    cmd_common = argparse.ArgumentParser(add_help=False)

    cmd_common.add_argument('passwdfile', type=str, help="password file", metavar="FILE")

    # Show file contents
    cmd_show = subcommands.add_parser("show", help="show file", parents=[cmd_common])
    cmd_show.set_defaults(func=show_file)

    # Add user command
    cmd_add = subcommands.add_parser("add", help="add a new user", parents=[cmd_common])

    cmd_add.add_argument('user', type=str, help="user name", metavar="USER")
    cmd_add.add_argument('passwd', type=str, help="user password", metavar="PASSWORD")
    cmd_add.add_argument('--flags', type=str, help="user flags")
    cmd_add.add_argument('--keyfunc', type=str, help="key derivation function")
    cmd_add.add_argument('--no-check', action="store_true", dest="nocheck", help="don't check for duplicates")
    cmd_add.set_defaults(func=add_user)

    # Update user flags or password command
    cmd_passwd = subcommands.add_parser("update", help="change user password", parents=[cmd_common])
    cmd_passwd.add_argument('user', type=str, help="user name", metavar="USER")
    cmd_passwd.add_argument('--passwd', type=str, help="new password", metavar="PASSWORD")
    cmd_passwd.add_argument('--flags', type=str, help="new flags")
    cmd_passwd.add_argument('--keyfunc', type=str, help="key derivation function")
    cmd_passwd.set_defaults(func=update_user)

    # Parse arguments
    args = vars(parser.parse_args())

    try:
        func = args.pop('func')
    except KeyError:
        parser.print_help()
        sys.exit(0)

    try:
        func(**args)
    except PasswdFileError as e:
        print(str(e))
        sys.exit(1)
    except IOError as e:
        print(str(e))
        sys.exit(1)


