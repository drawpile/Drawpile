#!/usr/bin/env python3
"""Autoban script for Drawpile dedicated server.
This script looks through the server's configuration database and
adds serverwide bans for users who have been repeatedly banned from sessions.

The serverwide ban duration depends on how many sessions the user was initially
banned from.

Basic usage: autoban.py config.db
Run "autoban.py -h" for list of supported command line arguments.
"""

import argparse
import sqlite3
import sys
import os

def autoban(conn, sessions, days, penalty, verbose=False):
    """Scan the server log for users who have been banned from at least the
    given number of different sessions in the last number of days and add
    them to the serverwide banlist.

    Returns the number of new bans added.

    Arguments:
    conn     -- the database connection
    sessions -- ban count treshold (must have been banned from this many sessions)
    days     -- timespan treshold (look for bans this many days in the past)
    penalty  -- serverwide ban will expire after this many days, multiplied per offense
    verbose  -- print extra info about discovered bans

    Returns:
    The number of new bans added
    """

    if sessions < 1:
        raise ValueError("Session count must be at least one.")
    if days < 1:
        raise ValueError("Must look back at least one day.")
    if penalty < 1:
        raise ValueError("Penalty time must be at least one day.")

    c = conn.cursor()

    # Get all in-session bans in the last days
    rows = c.execute("""SELECT user, session
    FROM serverlog
    WHERE timestamp>=datetime('now', ?)
    AND topic='Ban'""",
        ['-' + str(days) + ' days'])

    # Group bans by IP address
    users = {}
    for row in rows:
        try:
            userid, userip, username = row[0].split(';', 2)
        except ValueError:
            print("Invalid user (ID;IP;Name) triplet: " + row[0], file=sys.stderr)
            continue

        if userip not in users:
            users[userip] = {'names': set(), 'sessions': set()}
        users[userip]['names'].add(username)
        users[userip]['sessions'].add(row[1])
    
    # Gather a list of potential serverwide bans
    bans = []
    for ip, info in users.items():
        if verbose:
            print ('{banning}{ip} banned from {count} sessions using names "{names}"'.format(
                banning="PLONK! " if (len(info['sessions']) >= sessions) else "       ",
                ip=ip,
                names=', '.join(info['names']),
                count=len(info['sessions'])
                ))

        if len(info['sessions']) >= sessions:
            bans.append({
                'ip': ip,
                'expiration': '+{} days'.format(penalty * len(info['sessions'])),
                'comment': 'Banned from {count} sessions. Names used: {names}'.format(
                    names=', '.join(info['names']),
                    count=len(info['sessions'])
                    )
                })

    # Filter out existing bans from the list
    c.execute("SELECT ip FROM ipbans WHERE subnet=0 AND expires>datetime('now')")
    active_bans = set(x[0] for x in c.fetchall())
    bans = [b for b in bans if b['ip'] not in active_bans]

    # Insert new bans
    for b in bans:
        c.execute("INSERT INTO ipbans VALUES (:ip, 0, datetime('now', :expiration), :comment, datetime('now'))", b)
    conn.commit()

    return len(bans)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("database", help="configuration database")
    parser.add_argument("--sessions", type=int, default=3, help="number of sessions a user must be banned from")
    parser.add_argument("--days", type=int, default=2, help="number of days to look back")
    parser.add_argument("--penalty", type=int, default=5, help="serverwide ban duration per individual ban (days)")
    parser.add_argument('--verbose', '-v', default=False, action='store_true')
    args = parser.parse_args()

    if not os.path.isfile(args.database):
        print("{}: file not found!".format(args.database), file=sys.stderr)
        sys.exit(1)

    conn = sqlite3.connect(args.database)
    try:
        newbans = autoban(
                conn,
                sessions=args.sessions,
                days=args.days,
                penalty=args.penalty,
                verbose=args.verbose)

    except ValueError as e:
        print (str(e), file=sys.stderr)
        sys.exit(1)

    finally:
        conn.close()

    if newbans>0 or args.verbose:
        print ("{} new ban{} added".format(newbans, 's' if newbans!=1 else ''))

