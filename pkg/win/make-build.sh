#!/bin/bash

set -e

CMAKE=/usr/src/mxe/usr/bin/x86_64-w64-mingw32.shared-cmake

if [ ! -e $CMAKE ]; then
	echo "MinGW Cmake not found! Make sure to run this inside the build container."
	exit 1
fi

mkdir -p /Build
cd /Build
$CMAKE /Drawpile -DTOOLS=on -DWINTAB=on
make -j5

