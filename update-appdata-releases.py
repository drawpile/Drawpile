#!/usr/bin/env python3

# A script for updating the AppData file's <releases> section
# with the latest release from the Changelog

import re
import os
import hashlib
import xml.etree
import xml.etree.ElementTree as ET
from enum import Enum

CHANGELOG_FILE = 'ChangeLog'
APPDATA_FILE = 'src/desktop/appdata.xml'

class ChangeLogError(Exception):
    pass

class State(Enum):
    NEED_HEADER = 0
    SKIPPING = 1
    NEED_CHANGE = 2
    FOUND_CHANGE = 3

def read_changelog(filename):
    state = State.NEED_HEADER
    changelist = []

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                if state == State.SKIPPING:
                    state = State.NEED_HEADER
                    continue
                else:
                    break

            if state == State.NEED_HEADER:
                # Expect header
                m = re.match(r'^(\d{4}-\d{2}-\d{2}|Unreleased) Version (\d+\.\d+\.\d+(-[A-Za-z0-9.-]*)?(?:\\+[A-Za-z0-9.-]*)?)$', line)
                if not m:
                    raise ChangeLogError("Unparseable version line: " + line)

                date, version, version_tag = m.groups()
                if date == 'Unreleased':
                    print("Skipping unreleased changes")
                    state = State.SKIPPING
                else:
                    state = State.NEED_CHANGE

            elif state == State.SKIPPING:
                pass
            else:
                state = State.FOUND_CHANGE
                # Expect a change list entry
                if line[0] == '*':
                    changelist.append(line[1:].strip())
                else:
                    changelist[-1] = changelist[-1] + ' ' + line[1:].strip()

    if state != State.FOUND_CHANGE:
        raise ChangeLogError("ChangeLog contains no entries")

    return {
        'version': version,
        'type': 'stable' if version_tag is None else 'development',
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
    release = ET.Element('release',
        version=changes['version'],
        type=changes['type'],
        date=changes['date']
    )
    indent(release, 3, 2)
    url = ET.SubElement(release, 'url')
    indent(url, after=3)
    url.text = 'https://drawpile.net/news/release-%s/' % changes['version']

    description = ET.SubElement(release, 'description')
    indent(description, 4, 3)
    p = ET.SubElement(description, 'p')
    indent(p, after=4)
    p.text = 'Changes in this version:'
    ul = ET.SubElement(description, 'ul')
    indent(ul, 5, 3)
    for change in changes['changes']:
        li = ET.SubElement(ul, 'li')
        indent(li, after=5)
        li.text = change
    indent(ul[-1], after=4)

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

def indent(elem, inner = None, after = None):
    if inner != None:
        elem.text = '\n' + '  ' * inner
    if after != None:
        elem.tail = '\n' + '  ' * after

def update_release_artifacts(appdata):
    latest_release = appdata.find('releases')[0]
    version = latest_release.attrib['version']

    # List of downloadable binaries
    binaries = (
        ('x86_64-windows-msvc', 'win', f'Drawpile-{version}.msi'),
        # This is a bogus tuple (it should probably be x86_64-apple-darwin) but
        # it is what appstream validation demands
        ('x86_64-darwin-gnu', 'osx', f'Drawpile-{version}.dmg'),
        ('x86_64-linux-gnu', 'appimage', f'Drawpile-{version}-x86_64.AppImage'),
        ('source', 'src', f'drawpile-{version}.tar.xz'),
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
        artifacts_elem = ET.SubElement(latest_release, 'artifacts')
        indent(artifacts_elem, 4, 2)

    for platform, artifact in artifacts.items():
        if platform == 'source':
            attrs = {'type': 'source'}
            xpath = "artifact[@type='source']"
        else:
            attrs = {
                'type': 'binary',
                'platform': platform,
            }
            xpath = f"artifact[@type='binary'][@platform='{platform}']"

        elem = ET.Element('artifact', **attrs)
        indent(elem, 5, 4)
        location = ET.SubElement(elem, 'location')
        indent(location, after=4)
        location.text = 'https://drawpile.net/files/' + artifact['filename']
        if artifact['hash'] is not None:
            indent(location, after=5)
            checksum = ET.SubElement(elem, 'checksum', type='sha256')
            indent(checksum, after=5 if artifact['size'] is not None else 4)
            checksum.text = artifact['hash']
        if artifact['size'] is not None:
            indent(location, after=5)
            size = ET.SubElement(elem, 'size', type='download')
            indent(size, after=4)
            size.text = str(artifact['size'])

        for old in artifacts_elem.findall(xpath):
            artifacts_elem.remove(old)

        artifacts_elem.append(elem)

    if bool(artifacts):
        indent(latest_release.find('description'), after=3)
        indent(artifacts_elem[-1], after=3)
    else:
        indent(latest_release.find('description'), after=2)
        latest_release.remove(artifacts_elem)

    return artifacts.items()

if __name__ == '__main__':
    latestChanges = read_changelog(CHANGELOG_FILE)
    appdata = ET.parse(APPDATA_FILE)

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
        appdata.getroot().tail = '\n'
        appdata.write(APPDATA_FILE, encoding='utf-8', xml_declaration=True)
        print("Validating appdata file...")
        os.system('appstream-util validate-relax ' + APPDATA_FILE)
