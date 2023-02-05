#!/usr/bin/env python3

# A script for updating the AppData file's <releases> section
# with the latest release from the Changelog

from lxml import etree
from lxml.builder import E

import re
import os
import hashlib

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


def find_artifact(filename):
    path = 'artifacts/' + filename
    try:
        info = os.stat(path)
    except FileNotFoundError:
        return None

    with open(path, 'rb') as f:
        checksum = hashlib.sha256()
        while True:
            chunk = f.read(4096)
            if not chunk:
                break
            checksum.update(chunk)

    return {
        'filename': filename,
        'size': info.st_size,
        'hash': checksum.hexdigest(),
    }


def update_release_artifacts(appdata):
    latest_release = appdata.find('releases')[0]
    version = latest_release.attrib['version']

    # List of downloadable binaries
    binaries = (
        ('win64', 'win', f'drawpile-{version}-setup.exe'),
        ('macos', 'osx', f'Drawpile {version}.dmg'),
        ('source', 'src', f'drawpile-{version}.tar.gz'),
    )

    # Find metadata for binaries
    artifacts = {}
    for platform, prefix, filename in binaries:
        metadata = find_artifact(filename)
        if metadata:
            metadata['filename'] = prefix + '/' + metadata['filename']
            artifacts[platform] = metadata

    # Update artifacts element
    artifacts_elem = latest_release.find('artifacts')
    if artifacts_elem is None:
        artifacts_elem = E.artifacts()
        latest_release.append(artifacts_elem)

    for platform, artifact in artifacts.items():
        if platform == 'source':
            attrs = {'type': 'source'}
            xpath = "artifact[@type='source']"
        else:
            attrs = {
                'type': 'binary',
                'platform': platform,
            }
            xpath = f"artifact[@type='binary' and @platform='{platform}']"

        elem = E.artifact(
            E.location('https://drawpile.net/files/' + artifact['filename']),
            E.checksum(artifact['hash'], type='sha256'),
            E.size(str(artifact['size']), type='download'),
            **attrs
            )

        for old in artifacts_elem.xpath(xpath):
            artifacts_elem.remove(old)

        artifacts_elem.append(elem)

    return artifacts.items()

if __name__ == '__main__':
    latestChanges = read_changelog(CHANGELOG_FILE)
    appdata = etree.parse(
        APPDATA_FILE,
        etree.XMLParser(remove_blank_text=True)
    )

    changed = False

    if not add_release(appdata, latestChanges):
        print("Version %s already included in <releases>." % latestChanges['version'])

    else:
        print("Adding release", latestChanges['version'])
        changed = True

    release_artifacts = update_release_artifacts(appdata)
    if release_artifacts:
        print("Updated release artifacts:")
        for platform, ra in release_artifacts:
            print('\t', platform, ra['filename'], ra['hash'])

        changed = True
    else:
        print("No release artifacts")

    if changed:
        appdata.write(APPDATA_FILE, encoding='utf-8', xml_declaration=True, pretty_print=True)
        print("Validating appdata file...")
        os.system('appstream-util validate-relax ' + APPDATA_FILE)

