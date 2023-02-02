#!/usr/bin/env python3
"""Ban list analysis & autoban tool for Drawpile Dedicted server.

List bans:
    bantool.py <database file> list [--expired] [--format table|json|csv] [--ip IP]

Autoban users who have been banned often from individual sessions:
    bantool.py <database file> autoban [--dry-run] [--days 2] [--sessions 3] [--penalty 5]

    The --days parameter sets the number of days to look back in the log.
    The --session parameter sets the number of sessions a user must have been
    banned from to earn a serverwide ban.
    The --penalty parameter sets the ban duration in days per infraction.
    Use the --dry-run option to see what would happen without actually banning
    anyone.

Get info about a specific IP's ban status:
    bantool.py <database file> info <ip address>

Add a ban manually:
    bantool.py <database file> ban <ip address[/netmask]>

Remove a ban manually:
    bantool.py <database file> unban <ip address[/netmask] | all | expired>

    Note: autoban will restore all bans that match its criteria!
    Using the special target 'expired' will delete all expired ban entries
    from the database.

Clear all active bans:
    bantool.py <database file> unban all
"""

import argparse
import itertools
import sqlite3
import ipaddress
import json
import csv
import sys
import os

class BadArg(Exception):
    pass

# List bans
def command_list(db, *, format, expired=False, ip=None):
    """Show ban list

    Arguments:
    db      -- the database connection
    format  -- output format (table, json, csv)
    expired -- show expired bans too
    ip      -- show only entries matching this IP
    """

    sql_filter = ["1=1"]
    params = []

    if not expired:
        sql_filter.append("expires > current_timestamp")

    if ip:
        sql_filter.append("in_subnet(?, ip, subnet)")
        params.append(ip)

    query = "SELECT expires, added, ip, subnet, comment FROM ipbans " \
            "WHERE {filter} " \
            "ORDER BY expires DESC".format(
                filter=' AND '.join(sql_filter)
            )

    rows = db.execute(query, params)

    FORMATS[format]([r[0] for r in rows.description], rows)


def command_autoban(db, *, sessions, days, penalty, verbose=False, dryrun=False):
    """Scan the server log for users who have been banned from at least the
    given number of different sessions in the last number of days and add
    them to the serverwide banlist.

    Arguments:
    db       -- the database connection
    sessions -- ban count treshold (must have been banned from this many sessions)
    days     -- timespan treshold (look for bans this many days in the past)
    penalty  -- serverwide ban will expire after this many days, multiplied per offense
    verbose  -- print extra info about discovered bans
    dryrun   -- don't actually add any new bans (setting this implies verbose=True)
    """

    if sessions < 1:
        raise BadArg("Session count must be at least one.")
    if days < 1:
        raise BadArg("Must look back at least one day.")
    if penalty < 1:
        raise BadArg("Penalty time must be at least one day.")
    verbose = verbose or dryrun

    # Get all in-session bans in the last days
    rows = db.execute("""SELECT user, session
        FROM serverlog
        WHERE timestamp>=datetime('now', ?)
        AND topic='Ban'""",
        ['-' + str(days) + ' days']
    )

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
    verbose_output = []
    bans = []
    for ip, info in users.items():
        if verbose:
            verbose_output.append((
                ' X' if (len(info['sessions']) >= sessions) else '',
                ip,
                len(info['sessions']),
                ', '.join(info['names'])
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
    active_bans = db.execute("SELECT ip FROM ipbans WHERE subnet=0 AND expires>current_timestamp")
    active_bans = set(x[0] for x in active_bans)
    bans = [b for b in bans if b['ip'] not in active_bans]

    if verbose:
        print_table(('Ban', 'IP', 'Sessions', 'Names used'), verbose_output)
        print("---")
        print("Adding %d new serverwide bans." % len(bans))

    # Insert new bans
    if not dryrun:
        for b in bans:
            db.execute("INSERT INTO ipbans VALUES (:ip, 0, datetime('now', :expiration), :comment, datetime('now'))", b)
        db.commit()


def command_info(db, *, ip):
    """Get ban related information about the given IP address.
    """


    # Serverwide bans:
    banned_until = db.execute(
        "SELECT julianday(expires)-julianday('now'), comment FROM ipbans WHERE expires>current_timestamp AND in_subnet(?, ip, subnet)",
        (ip,)
    ).fetchone()

    print("IP address:", ip)
    if banned_until:
        print("Banned serverwide for %.1f more days" % banned_until[0])
        print("Reason:", banned_until[1])
    else:
        print("No serverwide ban")

    # Session bans
    rows = db.execute("""SELECT timestamp, user, session, message
        FROM serverlog
        WHERE topic='Ban' AND user LIKE ?""",
        ['%;' + ip + ';%']
    ).fetchall()

    if rows:
        print("Banned from %d sessions:" % len(rows))
        rows = map(lambda row: (row[0], row[1][row[1].rindex(';')+1:], *row[2:]), rows)
        print_table(("Timestamp", "Username", "Session ID", "Message"), rows)
    else:
        print("Not banned from any session recently")


def command_ban(db, *, ip, days, message=""):
    """Add a ban entry manually.
    """
    if '/' in ip:
        ip, subnet = ip.split('/')
        subnet = int(subnet)
    else:
        subnet = 0

    if subnet < 0 or subnet > 128:
        raise BadArg("Invalid subnet mask")

    if days < 1:
        raise BadArg("Ban duration must be at least one day")

    db.execute(
        "INSERT INTO ipbans (ip, subnet, expires, comment, added) " \
        "VALUES (:ip, :subnet, datetime('now', :expiration), :msg, current_timestamp)",
        {
            'ip': ip,
            'subnet': subnet,
            'expiration': '+%d days' % days,
            'msg': message,
        }
    )
    db.commit()


def command_unban(db, *, ip):
    """Remove all active ban entries for the given IP"""
    if ip == 'all':
        c = db.execute("DELETE FROM ipbans WHERE expires > current_timestamp")

    elif ip == 'expired':
        c = db.execute("DELETE FROM ipbans WHERE expires < current_timestamp")

    else:
        if '/' in ip:
            ip, subnet = ip.split('/')
            subnet = int(subnet)
        else:
            subnet = 0

        c = db.execute(
            "DELETE FROM ipbans WHERE expires > current_timestamp AND ip=? AND subnet=?",
            (ip, subnet)
        )
    db.commit()

    print("Removed %d entries" % c.rowcount)


# Output formatters
def print_table(header, rows):
    rows = list(rows)
    columns = [0] * len(header)

    for row in itertools.chain((header,), rows):
        for i, col in enumerate(row[:-1]):
            columns[i] = max(columns[i], len(str(col)))

    for row in itertools.chain((header,), rows):
        for width, col in zip(columns, row):
            val = str(col)
            sys.stdout.write(val)
            sys.stdout.write(' ' * (width - len(val) + 1))
        sys.stdout.write('\n')

def print_json(header, rows):
    json.dump(
        list(dict(zip(header, row)) for row in rows),
        sys.stdout,
        indent=0
    )

def print_csv(header, rows):
    writer = csv.writer(sys.stdout)
    writer.writerow(header)
    writer.writerows(rows)

FORMATS = {
    'table': print_table,
    'json': print_json,
    'csv': print_csv,
}


def _sqlite_in_subnet(ip, subnet, mask):
    try:
        ipaddr = ipaddress.ip_address(ip)
        if mask == 0:
            return ipaddr == ipaddress.ip_address(subnet)

        return ipaddr in ipaddress.ip_network(subnet + "/" + str(mask))
    except Exception as e:
        print(e)
        raise


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("database",
        help="configuration database")

    subparsers = parser.add_subparsers(help="sub-command help")

    # List bans
    cmd_list = subparsers.add_parser("list", help="List bans")
    cmd_list.add_argument("--format", type=str, default='table',
        help="Output format (%s)" % ', '.join(FORMATS.keys()))
    cmd_list.add_argument("--expired", dest="expired", action="store_true",
        help="Show expired bans")
    cmd_list.add_argument("--ip", type=str, default='',
        help="Show ban rules matching this IP")
    cmd_list.set_defaults(func=command_list)

    # Info about a specific IPs ban status
    cmd_info = subparsers.add_parser("info", help="Info about a specific IP address")
    cmd_info.add_argument("ip", type=str, help="IP address")
    cmd_info.set_defaults(func=command_info)

    # Autoban
    cmd_autoban = subparsers.add_parser("autoban", help="Autoban repeat offenders")
    cmd_autoban.add_argument("--sessions", type=int, default=3, help="number of sessions a user must be banned from")
    cmd_autoban.add_argument("--days", type=int, default=2, help="number of days to look back")
    cmd_autoban.add_argument("--penalty", type=int, default=5, help="serverwide ban duration per individual ban (days)")
    cmd_autoban.add_argument('--verbose', '-v', default=False, action='store_true')
    cmd_autoban.add_argument('--dry-run', dest="dryrun", default=False, action='store_true')
    cmd_autoban.set_defaults(func=command_autoban)

    # Manually add a ban
    cmd_ban = subparsers.add_parser("ban",
        help="Add a ban")
    cmd_ban.add_argument("ip", type=str,
        help="IP address (with optional subnet)")
    cmd_ban.add_argument("--days", type=int, default=365,
        help="Number of days the ban lasts for (default is 1 year)")
    cmd_ban.add_argument("--message", "-m", dest="message", type=str,
        default="Banned manually",
        help="Banlist comment")
    cmd_ban.set_defaults(func=command_ban)

    # Remove a ban
    cmd_unban = subparsers.add_parser("unban",
        help="Remove bans")
    cmd_unban.add_argument("ip", type=str,
        help="IP[/subnet] | all | expired")
    cmd_unban.set_defaults(func=command_unban)

    # Parse arguments
    args = vars(parser.parse_args())

    if 'func' not in args:
        parser.print_help()
        sys.exit(0)

    cmd = args.pop('func')
    dbfile = args.pop('database')

    try:
        if not os.path.isfile(dbfile):
            raise BadArg(dbfile + ": file not found!")

        conn = sqlite3.connect(dbfile)
        try:
            conn.create_function("in_subnet", 3, _sqlite_in_subnet)
            cmd(conn, **args)
        finally:
            conn.close()

    except BadArg as e:
        print(str(e), file=sys.stderr)
        sys.exit(1)

