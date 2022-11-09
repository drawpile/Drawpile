#!/bin/bash

set -e

CMAKE=/usr/src/mxe/usr/bin/x86_64-w64-mingw32.shared-cmake
RUST_PREFIX=/usr/src/mxe/usr/x86_64-pc-linux-gnu

export RUST_TARGET=x86_64-pc-windows-gnu
export RUSTC=${RUST_PREFIX}/bin/rustc
export CARGO=${RUST_PREFIX}/bin/cargo

if [ ! -e $CMAKE ]; then
	echo "MinGW Cmake not found! Make sure to run this inside the build container."
	exit 1
fi

mkdir -p /Build
cd /Build
$CMAKE /Drawpile -DCMAKE_BUILD_TYPE=Release -DTOOLS=on -DKIS_TABLET=on -DBUILD_LABEL="${BUILD_LABEL}" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
make -j5

