#!/usr/bin/env python3
"""Log analysis tool for Drawpile dedicated server.

Usage:

    logtool.py <database file> [--columns col1,col2,...] [--output <format>] [filters]
"""

import argparse
import itertools
import sqlite3
import json
import csv
import sys
import os

COLUMNS = ("timestamp", "level", "topic", "ip", "username", "session", "message")

class BadArg(Exception):
    pass

def logtool(db, *, columns, output_fn, ips=None, names=None, sessions=None,
        topics=None, after=None, before=None):
    # Select columns
    for i, col in enumerate(columns):
        if col not in COLUMNS:
            raise BadArg("Invalid column '%s'. Must be one of (%s)" % (col, ','.join(COLUMNS)))

        if col == 'username':
            columns[i] = "extract_username(user) as username"
        elif col == 'ip':
            columns[i] = "extract_ip(user) as ip"

    sql_filters = ["1=1"] # just so we can always have a WHERE clause
    params = []

    # Filter by IP
    if ips:
        sql_filters.append("extract_ip(user) IN (%s)" % (','.join('?' * len(ips))))
        params += ips

    # Filter by user name
    if names:
        sql_filters.append("lower(extract_username(user)) IN (%s)" % (','.join('?' * len(names))))
        params += names

    # Filter by session ID
    if sessions:
        sql_filters.append("session IN (%s)" % (','.join('?' * len(sessions))))
        params += sessions

    # Filter by log topic
    if topics:
        sql_filters.append("topic IN (%s)" % (','.join('?' * len(topics))))
        params += topics

    # Add time range filtering
    if after:
        sql_filters.append("datetime(timestamp) > ?")
        params.append(after)

    if before:
        sql_filters.append("datetime(timestamp) < ?")
        params.append(before)

    # Execute query and show results
    query = "SELECT {columns} FROM serverlog WHERE {where} ORDER BY timestamp".format(
        columns=','.join(columns),
        where=' AND '.join(sql_filters),
    )

    rows = db.execute(query, params)
    output_fn([r[0] for r in rows.description], rows)


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
    encoder = json.JSONEncoder(
        ensure_ascii=False,
        check_circular=False,
    )
    sys.stdout.write('[')
    first = True
    for row in rows:
        if first:
            first = False
        else:
            sys.stdout.write(',\n')

        sys.stdout.write(encoder.encode(dict(zip(header, row))))
    sys.stdout.write(']\n')

def print_csv(header, rows):
    writer = csv.writer(sys.stdout)
    writer.writerow(header)
    writer.writerows(rows)

FORMATS = {
    'table': print_table,
    'json': print_json,
    'csv': print_csv,
}


def _sqlite_extract_ip(value):
    if not isinstance(value, str):
        return ''

    idx1 = value.index(';')
    if idx1 < 0:
        return ''
    idx2 = value.index(';', idx1+1)

    return value[idx1+1:idx2]

def _sqlite_extract_username(value):
    if not isinstance(value, str):
        return ''

    idx = value.rindex(';')
    if idx < 0:
        return ''
    return value[idx+1:]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("database",
        help="configuration database")
    parser.add_argument("--columns", type=str, default='',
        help="List only selected columns")
    parser.add_argument("--format", type=str, default='table',
        help="Output format (%s)" % ', '.join(FORMATS.keys()))

    parser.add_argument("--ip", type=str, default='',
        help="Filter by IP address (comma separated list)")
    parser.add_argument("--name", type=str, default='',
        help="Filter by username (case insensitive comma separated list)")
    parser.add_argument("--session", type=str, default='',
        help="Filter by session ID (comma separated list)")
    parser.add_argument("--topic", type=str, default='',
        help="Filter by topic (comma separated list)")
    parser.add_argument("--after", type=str, default='',
        help="Show entries after this timestamp")
    parser.add_argument("--before", type=str, default='',
        help="Show entries before this timestamp")

    args = parser.parse_args()

    try:
        if not os.path.isfile(args.database):
            raise BadArg(args.database + ": file not found!")

        if args.columns:
            columns = args.columns.split(',')
        else:
            columns = list(COLUMNS)

        if args.format not in FORMATS:
            raise BadArg("Unknown output format: '%s'" % args.format)

        conn = sqlite3.connect(args.database)
        try:
            conn.create_function("extract_ip", 1, _sqlite_extract_ip)
            conn.create_function("extract_username", 1, _sqlite_extract_username)

            logtool(conn,
                columns=columns,
                output_fn=FORMATS[args.format],
                ips=args.ip.split(',') if args.ip else None,
                names=[name.lower() for name in args.name.split(',')] if args.name else None,
                sessions=args.session.split(',') if args.session else None,
                topics=args.topic.split(',') if args.topic else None,
                after=args.after,
                before=args.before,
            )
        except BrokenPipeError:
            pass
        finally:
            conn.close()

    except BadArg as e:
        print(str(e), file=sys.stderr)
        sys.exit(1)

