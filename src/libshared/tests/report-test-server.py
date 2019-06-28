#!/usr/bin/env python3
"""A HTTP server that receives abuse reports and prints them to stdout.

This is suitable for testing/development use only.
"""

from http.server import BaseHTTPRequestHandler, HTTPServer
import sys
import json

class ReportTestServer(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()

        self.wfile.write(b'send POST request here')

    def do_POST(self):
        print("Received POST:", self.path)
        print("Authorization:", self.headers['Authorization'])
        if self.headers['Content-Type'] != 'application/json':
            print ("Wrong content type:", self.headers['Content-Type'], "(expected application/json)")
            self.send_response(400)
            self.end_headers()
            return

        data = self.rfile.read(int(self.headers['Content-Length']))
        body = json.loads(data)
        print_report(body)
        print("-" * 70)

        self.send_response(204)
        self.end_headers()

def print_report(report):
    main_schema = (
        ('Session ID', 'session', str),
        ('Session title', 'sessionTitle', str),
        ('Reporter', 'user', str),
        ('Auth', 'auth', bool),
        ('IP address', 'ip', str),
        ('Perpetrator', 'perp', int), # this is actually an optional field
        ('Offset', 'offset', int),
        ('Message', 'message', str),
    )
    user_schema = (
        ('\tID', 'id', int),
        ('\tName', 'name', str),
        ('\tAuth', 'auth', bool),
        ('\tOp', 'op', bool),
        ('\tIP', 'ip', str),
    )

    def print_schema(d, schema):
        for label, key, expected_type in schema:
            try:
                val = d[key]
                if not isinstance(val, expected_type):
                    val = str(val) + ' [WRONG TYPE]'
            except KeyError:
                val = '[MISSING]'

            print(label + ':', val)

    print_schema(report, main_schema)

    try:
        users = report['users']
    except KeyError:
        print("[USER LIST MISSING]")
    else:
        if not isinstance(users, list):
            print("Users:", users, "[NOT A LIST!]")
        else:
            print("Users:")
            for user in users:
                if 'id' in user and 'perp' in report and user['id'] == report['perp']:
                    print("[The target of this report]")
                print_schema(user, user_schema)
                print("\t---")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        addr = ('localhost', int(sys.argv[1]))
    else:
        addr = ('localhost', 8080)

    print ("Listening on port", addr[1])
    httpd = HTTPServer(addr, ReportTestServer)
    httpd.serve_forever()

