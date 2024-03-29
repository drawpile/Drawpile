#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds Emscripten dependencies locally. Only tested to work on Linux.
QT_VERSION_MAJOR=6
QT_VERSION_MINOR=6
QT_VERSION_PATCH=3
QT_VERSION="$QT_VERSION_MAJOR.$QT_VERSION_MINOR.$QT_VERSION_PATCH"
LIBZIP_VERSION='1.9.2'

SCRIPT_DIR="$(readlink -f "$(dirname "$0")")"
if [[ -z "$SCRIPT_DIR" ]]; then
    exit 1
fi
. "$SCRIPT_DIR/common.bash"

HOST_DIR="$SCRIPT_DIR/buildhost/host"
EM_DIR="$SCRIPT_DIR/$build_dir/em"
HOSTPREFIX_DIR="$SCRIPT_DIR/buildhost/hostprefix"
EMPREFIX_DIR="$SCRIPT_DIR/$build_dir/emprefix"

set -xe

if [[ ! -e "$HOST_DIR/done" ]]; then
    mkdir -p "$HOST_DIR"
    cd "$HOST_DIR"
    cmake \
        -DBUILD_TYPE=release \
        -DCMAKE_INSTALL_PREFIX="$HOSTPREFIX_DIR" \
        -DQT_VERSION="$QT_VERSION" \
        -DMULTIMEDIA=OFF \
        -DSHADERTOOLS=ON \
        -DSVG=OFF \
        -DIMAGEFORMATS=OFF \
        -DTRANSLATIONS=OFF \
        -P "$SRC_DIR/.github/scripts/build-qt.cmake"
    touch "$HOST_DIR/done"
fi

mkdir -p "$EM_DIR"
cd "$EM_DIR"

# Make emcc build the zlib port. Needs a dummy compilation.
emcc -sUSE_ZLIB=1 -o "$EM_DIR/port.wasm" "$SCRIPT_DIR/port.c"

cmake \
    "-DBUILD_TYPE=$build_type" \
    -DCMAKE_INSTALL_PREFIX="$EMPREFIX_DIR" \
    -DQT_VERSION="$QT_VERSION" \
    -DEMSCRIPTEN=ON \
    -DEMSCRIPTEN_THREADS=ON \
    -DEMSCRIPTEN_HOST_PATH="$HOSTPREFIX_DIR" \
    -DKEEP_SOURCE_DIRS=ON \
    -DKEEP_BINARY_DIRS=ON \
    -P "$SRC_DIR/.github/scripts/build-qt.cmake"

wget "https://libzip.org/download/libzip-$LIBZIP_VERSION.tar.xz"
tar xf "libzip-$LIBZIP_VERSION.tar.xz"
rm "libzip-$LIBZIP_VERSION.tar.xz"
"$SCRIPT_DIR/$build_dir/emprefix/bin/qt-cmake" \
    "-DCMAKE_BUILD_TYPE=$cmake_build_type" \
    -DBUILD_TOOLS=OFF \
    -DBUILD_REGRESS=OFF \
    -DBUILD_DOC=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DENABLE_COMMONCRYPTO=OFF \
    -DENABLE_GNUTLS=OFF \
    -DENABLE_MBEDTLS=OFF \
    -DENABLE_OPENSSL=OFF \
    -DENABLE_WINDOWS_CRYPTO=OFF \
    -DENABLE_BZIP2=OFF \
    -DENABLE_LZMA=OFF \
    -DENABLE_ZSTD=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_C_FLAGS=-pthread \
    -DCMAKE_INSTALL_PREFIX="$EMPREFIX_DIR" \
    -S "libzip-$LIBZIP_VERSION" \
    -B "libzip-$LIBZIP_VERSION-build" \
    -G Ninja
cmake --build "libzip-$LIBZIP_VERSION-build"
cmake --install "libzip-$LIBZIP_VERSION-build"

set +xe

echo
echo "Setup for Emscripten $build_mode dependencies complete."
echo
echo "If you haven't done so, you must install the required Rust stuff:"
echo "    rustup install nightly"
echo "    rustup +nightly target add wasm32-unknown-unknown"
echo "    rustup component add rust-src --toolchain nightly"
echo
echo "To configure and build Drawpile now, run:"
echo "    $SCRIPT_DIR/configure.bash $build_mode"
echo "    cmake --build $buildem_dir"
echo
