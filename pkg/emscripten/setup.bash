#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds Emscripten dependencies locally. Only tested to work on Linux.
QT_VERSION_MAJOR=6
QT_VERSION_MINOR=7
QT_VERSION_PATCH=2
QT_VERSION="$QT_VERSION_MAJOR.$QT_VERSION_MINOR.$QT_VERSION_PATCH"
LIBZIP_VERSION='1.10.1'
LIBWEBP_VERSION='1.4.0'

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

pushd "$SCRIPT_DIR/../../src/desktop/wasm"
npm install --include=dev
popd

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
        -DWEBSOCKETS=OFF \
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

wget -O "libwebp-$LIBWEBP_VERSION.tar.gz" "https://github.com/webmproject/libwebp/archive/refs/tags/v$LIBWEBP_VERSION.tar.gz"
tar xf "libwebp-$LIBWEBP_VERSION.tar.gz"
rm "libwebp-$LIBWEBP_VERSION.tar.gz"
"$SCRIPT_DIR/$build_dir/emprefix/bin/qt-cmake" \
    "-DCMAKE_BUILD_TYPE=$cmake_build_type" \
    -DWEBP_BUILD_ANIM_UTILS=OFF \
    -DWEBP_BUILD_CWEBP=OFF \
    -DWEBP_BUILD_DWEBP=OFF \
    -DWEBP_BUILD_GIF2WEBP=OFF \
    -DWEBP_BUILD_IMG2WEBP=OFF \
    -DWEBP_BUILD_VWEBP=OFF \
    -DWEBP_BUILD_WEBPINFO=OFF \
    -DWEBP_BUILD_LIBWEBPMUX=ON \
    -DWEBP_BUILD_WEBPMUX=OFF \
    -DWEBP_BUILD_EXTRAS=OFF \
    -DWEBP_BUILD_WEBP_JS=OFF \
    -DWEBP_NEAR_LOSSLESS=ON \
    -DWEBP_ENABLE_WUNUSED_RESULT=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_C_FLAGS=-pthread \
    -DCMAKE_INSTALL_PREFIX="$EMPREFIX_DIR" \
    -S "libwebp-$LIBWEBP_VERSION" \
    -B "libwebp-$LIBWEBP_VERSION-build" \
    -G Ninja
cmake --build "libwebp-$LIBWEBP_VERSION-build"
cmake --install "libwebp-$LIBWEBP_VERSION-build"

set +xe

echo
echo "Setup for Emscripten $build_mode dependencies complete."
echo
echo "To configure and build Drawpile now, run:"
echo "    $SCRIPT_DIR/configure.bash $build_mode"
echo "    cmake --build $buildem_dir"
echo
