#!/bin/bash

set -e

if [ "${QTDIR+}" == "" ]; then
	QTDIR="$HOME/Qt/5.9.7/clang_64"
fi

VERSION=$(grep DRAWPILE_VERSION ../../CMakeLists.txt | cut -d \" -f 2)
TITLE="Drawpile $VERSION"

if [ ! -d "$QTDIR" ]; then
	echo "$QTDIR not found!"
	exit 1
fi

if [ "$(which appdmg)" == "" ]; then
	echo "Appdmg not found!"
	echo "Run npm install -g appdmg"
	exit 1
fi

if [ -d build ]; then
	echo "Old build directory exists!"
	echo "Run 'rm -rf build' and try again."
	exit 1
fi

# Build
mkdir build
pushd build
cmake ../../../ \
	"-DCMAKE_PREFIX_PATH=$QTDIR" \
	-DSERVER=OFF \
	-DCMAKE_BUILD_TYPE=Release
make

# Remove version string from the binary
pushd bin
pushd Drawpile.app/Contents/MacOS
BINFILE="$(readlink -n Drawpile)"
rm Drawpile
mv "$BINFILE" Drawpile
popd

# Bundle frameworks
"$QTDIR/bin/macdeployqt" Drawpile.app
popd
popd

# Package the app in a dmg archive
appdmg spec.json build/bin/Drawpile.dmg

