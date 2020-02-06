#!/bin/bash

set -e

### VERSIONS TO DOWNLOAD
GIFLIB_URL=https://sourceforge.net/projects/giflib/files/giflib-5.1.4.tar.gz/download
MINIUPNPC_URL=http://miniupnp.free.fr/files/download.php?file=miniupnpc-2.1.tar.gz
LIBVPX_URL=https://github.com/webmproject/libvpx/archive/v1.8.0.zip
ECM_URL=https://download.kde.org/stable/frameworks/5.64/extra-cmake-modules-5.64.0.tar.xz
KARCHIVE_URL=https://download.kde.org/stable/frameworks/5.64/karchive-5.64.0.tar.xz
KDNSSD_URL=https://download.kde.org/stable/frameworks/5.64/kdnssd-5.64.0.tar.xz
KEYCHAIN_URL=https://github.com/frankosterfeld/qtkeychain/archive/v0.10.0.zip

### Build flags
export CFLAGS=-mmacosx-version-min=10.10
export CXXFLAGS=-mmacosx-version-min=10.10

### GENERIC FUNCTIONS
function download_package() {
	URL="$1"
	OUT="$2"

	if [ -f "$OUT" ]
	then
		echo "$OUT already downloaded. Skipping..."
	else
		curl -L "$URL" -o "$OUT"
	fi
}

function install_package() {
	if [ -d $1-* ]; then
		echo "Build directory for $1 already exists. Skipping..."
		return
	fi
	if [ -f "$1.zip" ]; then
		unzip -q "$1.zip"
	elif [ -f "$1.tar.gz" ]; then
		tar xfz "$1.tar.gz"
	elif [ -f "$1.tar.xz" ]; then
		tar xfJ "$1.tar.xz"
	else
		echo "BUG: Unhandled package archive format $1"
		exit 1
	fi
	pushd $1-*
	build_$2
	popd
}

### PACKAGE SPECIFIC BUILD SCRIPTS
function build_autoconf() {
	./configure "--prefix=$QTPATH"
	make
	make install
}

function build_autoconf_libvpx() {
	./configure "--prefix=$QTPATH" --disable-vp8 --disable-vp9-decoder
	make
	make install
}

function build_justmakeinstall() {
	INSTALLPREFIX="$QTPATH" make install
}

function build_cmake() {
	mkdir build
	cd build
	cmake .. "-DCMAKE_PREFIX_PATH=$QTPATH" "-DCMAKE_INSTALL_PREFIX=$QTPATH"
	make
	make install
}

### MAIN SCRIPT STARTS HERE
if [ -z "$QTPATH" ]
then
	echo "QTPATH environment variable not set"
	exit 1
fi

if [ ! -d "$QTPATH" ]
then
	echo "$QTPATH is not a directory!"
	exit 1
fi

echo "Dependencies will be downloaded to $(pwd)/deps and installed to $QTPATH."
echo "Write 'ok' to continue"

read confirmation

if [ "$confirmation" != "ok" ]
then
	echo "Cancelled."
	exit 0
fi

mkdir -p deps
cd deps

# Download dependencies
download_package "$GIFLIB_URL" giflib.tar.gz
download_package "$MINIUPNPC_URL" miniupnpc.tar.gz
download_package "$LIBVPX_URL" libvpx.zip
download_package "$ECM_URL" extra-cmake-modules.tar.xz
download_package "$KARCHIVE_URL" karchive.tar.xz
download_package "$KDNSSD_URL" kdnssd.tar.xz
download_package "$KEYCHAIN_URL" qtkeychain.zip

# Make sure we have the right versions (and they haven't been tampered with)
shasum -a 256 -c ../deps.sha256

# Build and install
install_package giflib autoconf
install_package miniupnpc justmakeinstall
install_package libvpx autoconf_libvpx
install_package extra-cmake-modules cmake
install_package karchive cmake
install_package kdnssd cmake
install_package qtkeychain cmake

