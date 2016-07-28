#!/usr/bin/python3
"""
Display a summary of kicked users based on server logs.
"""

import subprocess
import re

def get_kick_stats():
    # Get list of kicks
    proc = subprocess.Popen(["./kicks.sh"], shell=True, stdout=subprocess.PIPE)
    procout, procin = proc.communicate()

    # Gather statistics
    RE = re.compile(r'\#\d+ \[(?P<ip>[^]]+)\] (?P<nick>[^@]+)@(?P<session>[^:]+): Kicked by "(?P<kicker>[^"]+)"')
    kicks = {}
    for line in procout.decode('utf-8').split('\n'):
        if not line:
            continue
        m = RE.search(line)
        if not m:
            print ("Unparseable line:", line)
            continue

        ip = m.group('ip')
        if ip not in kicks:
            kicks[ip] = {
                'ip': ip,
                'nicks': set(),
                'sessions': set(),
                'kickers': set(),
                'count': 0
            }

        kicks[ip]['nicks'].add(m.group('nick'))
        kicks[ip]['sessions'].add(m.group('session'))
        kicks[ip]['kickers'].add(m.group('kicker'))
        kicks[ip]['count'] += 1

    return kicks

def print_kick_stats(kicks):
    kicks = sorted(kicks.values(), key=lambda x : x['count'], reverse=True)

    for k in kicks:
        print("{ip} kicked {count} times from {sessions} session(s). Nicks used: {nicks}. Kicked by {kickers}".format(
            ip=k['ip'],
            count=k['count'],
            sessions=len(k['sessions']),
            nicks=', '.join(k['nicks']),
            kickers=', '.join(k['kickers']),
            ))

if __name__ == '__main__':
    print_kick_stats(get_kick_stats())

