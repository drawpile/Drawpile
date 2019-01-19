#!/bin/bash

set -e

GIFLIB_URL=https://sourceforge.net/projects/giflib/files/giflib-5.1.4.tar.gz/download
MINIUPNPC_URL=http://miniupnp.free.fr/files/download.php?file=miniupnpc-2.1.tar.gz
LIBSODIUM_URL=https://download.libsodium.org/libsodium/releases/libsodium-1.0.17.tar.gz
ECM_URL=https://download.kde.org/stable/frameworks/5.54/extra-cmake-modules-5.54.0.tar.xz
KARCHIVE_URL=https://download.kde.org/stable/frameworks/5.54/karchive-5.54.0.tar.xz
KDNSSD_URL=https://download.kde.org/stable/frameworks/5.54/kdnssd-5.54.0.tar.xz

if [ -z "$QTPATH" ]
then
	echo "QTPATH environment variable not set"
	exit 1
fi

echo "Dependencies will be downloaded to $(pwd)/deps and installed to QTPATH."
echo "Write 'ok' to continue"

read confirmation

if [ "$confirmation" != "ok" ]
then
	echo "Cancelled."
	exit 0
fi

#mkdir deps
cd deps

# Download dependencies
curl -L "$GIFLIB_URL" -o giflib.tar.gz
curl -L "$MINIUPNPC_URL" -o miniupnpc.tar.gz
curl -L "$LIBSODIUM_URL" -o libsodium.tar.gz
curl -L "$ECM_URL" -o ecm.tar.xz
curl -L "$KARCHIVE_URL" -o karchive.tar.xz
curl -L "$KDNSSD_URL" -o kdnssd.tar.xz

# Install extra-cmake-modules
tar xf ecm.tar.xz
cd extra-cmake-modules*
cmake "-DCMAKE_INSTALL_PREFIX=$QTPATH"
make
make install
cd ..

# Install karchive
tar xf karchive.tar.xz
cd karchive-*
mkdir build
cd build
cmake .. "-DCMAKE_PREFIX_PATH=$QTPATH" "-DCMAKE_INSTALL_PREFIX=$QTPATH"
make
make install
cd ../..

# Install KDNSSD
tar xf kdnssd.tar.xz
cd kdnssd-*
mkdir build
cd build
cmake .. "-DCMAKE_PREFIX_PATH=$QTPATH" "-DCMAKE_INSTALL_PREFIX=$QTPATH"
make
make install
cd ../..

# Install GIFLIB
tar xf giflib.tar.gz
cd giflib-*
./configure "--prefix=$QTPATH"
make
make install
cd ..

# Install miniupnpc
tar xf miniupnpc.tar.gz
cd miniupnpc-*
INSTALLPREFIX="$QTPATH" make install


