#!/usr/bin/python3
"""
Automatically ban users who have been kicked too many times.
"""

from kickstats import get_kick_stats
from datetime import date

BANLIST_FILE = "ip.banlist"
AUTOBAN_LIMIT = 3

def get_banlist():
    banlist = set()
    with open(BANLIST_FILE, 'r') as blf:
        for line in blf:
            line = line.strip()
            if not line or line[0] == '#':
                continue
            banlist.add(line)

    return banlist


def make_banlist(stats, oldbans):
    newbans = []
    for ip in stats.values():
        if ip['count'] > AUTOBAN_LIMIT and ip['ip'] not in oldbans:
            newbans.append({
                'nicks': ip['nicks'],
                'ip': ip['ip'],
                'count': ip['count'],
            })
    return newbans


def append_new_bans(banlist):
    today = str(date.today())

    with open(BANLIST_FILE, 'a') as blf:
        for b in banlist:
            blf.write('\n# {nicks} ({date})\n{ip}\n'.format(
                nicks=', '.join(b['nicks']),
                date=today,
                ip=b['ip']
            ))


if __name__ == '__main__':
    banlist = get_banlist()
    stats = get_kick_stats()
    
    new_bans = make_banlist(stats, banlist)
    if new_bans:
        print("Adding {} new bans:".format(len(new_bans)))
        for b in new_bans:
            print ('{ip} (kicked {count} times)'.format(**b))
        append_new_bans(new_bans)
    else:
        print("No new bans to add.")

