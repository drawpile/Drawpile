#!/usr/bin/env python3

# A script for updating the AppData file's <releases> section
# with the latest release from the Changelog

import argparse
import re
import os
import hashlib
import xml.etree
import xml.etree.ElementTree as ET
from enum import Enum
from pathlib import Path

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
    path = (Path('artifacts') / filename).resolve()
    info = os.stat(path)

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

def update_release_artifacts(appdata, include_legacy_platform_ids):
    latest_release = appdata.find('releases')[0]
    version = latest_release.attrib['version']

    # List of downloadable binaries
    binaries = (
        ('x86_64-windows-msvc', 'win64', f'Drawpile-{version}-x86_64.msi'),
        ('i386-windows-msvc', 'win32', f'Drawpile-{version}-x86.msi'),
        ('x86_64-darwin-gnu', 'macos', f'Drawpile-{version}-x86_64.dmg'),
        ('aarch64-darwin-gnu', '', f'Drawpile-{version}-arm64.dmg'),
        ('x86_64-linux-gnu', '', f'Drawpile-{version}-x86_64.AppImage'),
        ('aarch64-linux-android', '', f'Drawpile-{version}-arm64-v8a.apk'),
        ('arm-linux-android', '', f'Drawpile-{version}-armeabi-v7a.apk'),
        ('source', '', f'Drawpile-{version}.tar.gz'),
    )

    # Find metadata for binaries
    artifacts = {}
    for platform, legacy_platform, filename in binaries:
        metadata = find_artifact(filename)
        if metadata:
            metadata['filename'] = metadata['filename']
            if include_legacy_platform_ids and legacy_platform:
                artifacts[legacy_platform] = metadata
            else:
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
            download = f'''https://github.com/drawpile/Drawpile/archive/refs/tags/{version}.tar.gz'''
        else:
            attrs = {
                'type': 'binary',
                'platform': platform,
            }
            xpath = f"artifact[@type='binary'][@platform='{platform}']"
            download = f'''https://github.com/drawpile/Drawpile/releases/download/{version}/{artifact['filename']}'''

        elem = ET.Element('artifact', **attrs)
        indent(elem, 5, 4)
        location = ET.SubElement(elem, 'location')
        indent(location, after=4)
        location.text = download
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
    parser = argparse.ArgumentParser(
        description = 'Updates appdata.xml by parsing the latest entry in ChangeLog',
    )
    parser.add_argument('--changelog', default=(Path(__file__).parent / '../ChangeLog').resolve(),
                        help='path to ChangeLog file')
    parser.add_argument('--in', dest='in_file', default='',
                        help='path to input appdata.xml')
    parser.add_argument('--out', dest='out_file', default='',
                        help='path to output appdata.xml')
    # Needed for compatibility with Drawpile 2.1 auto-update mechanism,
    # otherwise it will find no matching updates ever since it expects platform
    # values that are not valid triplets and so violate the AppStream spec.
    # Files generated with this option should only ever be uploaded to
    # drawpile.net
    parser.add_argument('--legacy', action='store_true',
                        help='emit legacy platform IDs')
    parser.add_argument('--artifacts', action='store_true',
                        help='create <artifacts> element')
    args = parser.parse_args()

    default_appdata_name = 'net.drawpile.drawpile.appdata.xml' if args.legacy else 'drawpile.appdata.xml'
    default_appdata = (Path(__file__).parent / f'../src/desktop/{default_appdata_name}').resolve()

    if not args.in_file:
        args.in_file = default_appdata
    if not args.out_file:
        args.out_file = args.in_file

    latestChanges = read_changelog(args.changelog)
    appdata = ET.parse(args.in_file)

    changed = False

    if not add_release(appdata, latestChanges):
        print("Version %s already included in <releases>." % latestChanges['version'])
    else:
        print("Adding release", latestChanges['version'])
        changed = True

    if args.artifacts:
        release_artifacts = update_release_artifacts(appdata, args.legacy)
        if release_artifacts:
            print("Updated release artifacts:")
            for platform, ra in release_artifacts:
                print('\t', platform, ra['filename'], ra['hash'])
            changed = True
        else:
            print("No release artifacts")
    else:
        latest_release = appdata.find('releases')[0]
        indent(latest_release.find('description'), after=2)

    if changed or args.in_file != args.out_file:
        appdata.getroot().tail = '\n'
        appdata.write(args.out_file, encoding='utf-8', xml_declaration=True)
        # The header format is hard-coded to use single quotes, so hack it.
        if args.legacy:
            with open(args.out_file, 'r') as f:
                content = f.read()
            with open(args.out_file, 'w') as f:
                f.write(content.replace(
                    "<?xml version='1.0' encoding='utf-8'?>",
                    '<?xml version="1.0" encoding="UTF-8"?>',
                    1))
        print("Validating appdata file...")
        os.system(f'appstream-util validate-relax {args.out_file}')
