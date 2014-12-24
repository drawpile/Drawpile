#!/bin/bash

set -e

QTDIR="$HOME/Qt/5.4/clang_64"
TITLE="Drawpile 0.9.6"

# Create build directory
mkdir -p mac-deploy
cd mac-deploy

# Build the app
cmake .. "-DCMAKE_PREFIX_PATH=$QTDIR"
make

"$QTDIR/bin/macdeployqt" bin/Drawpile.app

# Construct app bundle
# See: http://stackoverflow.com/questions/96882/how-do-i-create-a-nice-looking-dmg-for-mac-os-x-using-command-line-tools

mkdir dmg
cp -r bin/Drawpile.app dmg

hdiutil create -srcfolder dmg -volname "$TITLE" -fs HFS+ \
      -fsargs "-c c=64,a=16,e=16" -format UDRW -size 128m pack.temp.dmg

# todo: add styling here

hdiutil convert pack.temp.dmg -format UDZO -imagekey zlib-level=9 -o "${TITLE}.dmg"
rm -f pack.temp.dmg 
rm -rf dmg

