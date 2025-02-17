#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads the SQLite amalgamation build's sqlite.h and sqlite.c, as well as
# the sqlite3recover.h, sqlite3recover.c and dbdata.c from the recover extension
# given a release year and SQLite version. Check the downloads on sqlite.org for
# which version and which year the current version uses.
set -e

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 YEAR SQLITE_VERSION" 2>&1
    exit 2
fi

cd "$(dirname "$0")"
year="$1"
version="$2"
amalgamation="sqlite-amalgamation-$version"
src="sqlite-src-$version"

mkdir -v "$version.tmp"
cd "$version.tmp"

curl -O "https://www.sqlite.org/$year/$amalgamation.zip"
unzip "$amalgamation.zip"
rm -rfv "$amalgamation.zip"
mv -v "$amalgamation/sqlite3.c" ../sqlite3.c
mv -v "$amalgamation/sqlite3.h" ../sqlite3.h
rm -rfv "$amalgamation"

curl -O "https://www.sqlite.org/$year/$src.zip"
unzip "$src.zip"
rm -rfv "$src.zip"
mv -v "$src/ext/recover/dbdata.c" ../dbdata.c
mv -v "$src/ext/recover/sqlite3recover.c" ../sqlite3recover.c
mv -v "$src/ext/recover/sqlite3recover.h" ../sqlite3recover.h
rm -rfv "$src"

cd ..
rmdir "$version.tmp"

