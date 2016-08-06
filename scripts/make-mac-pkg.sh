#!/bin/bash

set -e

QTDIR="$HOME/Qt/5.7/clang_64"
VERSION=$(grep DRAWPILE_VERSION CMakeLists.txt | cut -d \" -f 2)
TITLE="Drawpile $VERSION"

# Create build directory
mkdir -p mac-deploy
cd mac-deploy

# Build the app
cmake .. "-DCMAKE_PREFIX_PATH=$QTDIR" -DSERVER=off -DCMAKE_BUILD_TYPE=Release
make

# Remove version string from the binary
cd bin/Drawpile.app/Contents/MacOS
BINFILE="$(readlink -n Drawpile)"
rm Drawpile
mv "$BINFILE" Drawpile
cd -
cd bin

"$QTDIR/bin/macdeployqt" Drawpile.app -dmg

mv Drawpile.dmg "Drawpile $VERSION.dmg"

