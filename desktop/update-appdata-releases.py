#!/usr/bin/env python3

# A script for updating the AppData file's <releases> section
# with the latest release from the Changelog

from lxml import etree
from lxml.builder import E

import re
import os

CHANGELOG_FILE = '../ChangeLog'
APPDATA_FILE = 'net.drawpile.drawpile.appdata.xml'


class ChangeLogError(Exception):
    pass

def read_changelog(filename):
    state = 0
    changelist = []

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                break

            if state == 0:
                # Expect header
                m = re.match(r'^(\d{4}-\d{2}-\d{2}) Version (\d+\.\d+\.\d+)$', line)
                if not m:
                    raise ChangeLogError("Unparseable version line: " + line)

                date, version = m.groups()
                state = 1

            elif state == 1:
                # Expect a change list entry
                if line[0] == '*':
                    changelist.append(line[1:].strip())
                else:
                    changelist[-1] = changelist[-1] + ' ' + line[1:].strip()

    return {
        'version': version,
        'date': date,
        'changes': changelist
    }

def add_release(appdata, changes):
    releases = appdata.find('releases')

    # First, check that the latest version is not already listed
    for release in releases:
        if release.attrib['version'] == changes['version']:
            return False

    # Create the new release entry and insert it
    release = E.release(
        E.url('https://drawpile.net/news/release-%s/' % changes['version']),
        E.description(
            E.p("Changes in this version:"),
            E.ul(*[E.li(change) for change in changes['changes']])
        ),
        version=changes['version'],
        date=changes['date']
    )

    releases.insert(0, release)
    return True

if __name__ == '__main__':
    latestChanges = read_changelog(CHANGELOG_FILE)
    appdata = etree.parse(
        APPDATA_FILE,
        etree.XMLParser(remove_blank_text=True)
    )

    if not add_release(appdata, latestChanges):
        print("Version %s already included in <releases>, nothing to do." % latestChanges['version'])

    else:
        print("Adding release", latestChanges['version'])
        appdata.write(APPDATA_FILE, encoding='utf-8', xml_declaration=True, pretty_print=True)
        print("Validating appdata file...")
        os.system('appstream-util validate-relax ' + APPDATA_FILE)

