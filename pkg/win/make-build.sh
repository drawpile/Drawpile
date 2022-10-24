#!/bin/bash

set -e

CMAKE=/usr/src/mxe/usr/bin/x86_64-w64-mingw32.shared-cmake

if [ ! -e $CMAKE ]; then
	echo "MinGW Cmake not found! Make sure to run this inside the build container."
	exit 1
fi

mkdir -p /BuildDrawdance
cd /BuildDrawdance
$CMAKE /drawdance -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DUSE_ADDRESS_SANITIZER=OFF -DUSE_CLANG_TIDY=OFF -DUSE_GENERATORS=OFF -DUSE_STRICT_ALIASING=ON -DBUILD_APPS=OFF -DBUILD_TESTS=OFF -DTHREAD_IMPL=QT -DZIP_IMPL=KARCHIVE -DXML_IMPL=QT -DENABLE_HISTORY_DUMP=ON
make

mkdir -p /Build
cd /Build
$CMAKE /Drawpile -DCMAKE_BUILD_TYPE=Release -DTOOLS=on -DKIS_TABLET=on -DBUILD_LABEL="${BUILD_LABEL}" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DDRAWDANCE_EXPORT_PATH=/BuildDrawdance/Drawdance.cmake
make -j5

