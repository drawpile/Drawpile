#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Performs local Emscripten builds. Only tested to work on Linux.

# Qt strongly suggests using the same Emscripten version they use.
# TODO: Figure out why Qt 6.8.2 is busted. When e.g. operating sliders in the
# brush dock, nothing updates until something else in the UI is fiddled with.
# It also spews lots of warnings about glTexImage2D going out of bounds.
QT_VERSION_MAJOR=6
QT_VERSION_MINOR=7
QT_VERSION_PATCH=2
QT_VERSION="$QT_VERSION_MAJOR.$QT_VERSION_MINOR.$QT_VERSION_PATCH"
REQUIRED_EMSCRIPTEN_VERSION='3.1.50'

carp() {
    echo 1>&2
    echo "$@" 1>&2
}

croak() {
    carp "$@"
    echo 1>&2
    exit 1
}

croak_with_usage() {
    carp
    carp "Usage:"
    carp "    $0 hostsetup"
    carp "        to set up Qt on this host to be able to cross-compile"
    carp "    $0 emsetup"
    carp "        to set up the Emscripten dependencies"
    carp "    $0 configure"
    carp "        to configure the cmake build of Drawpile"
    carp
    carp "The following environment variables can be set:"
    carp "    BUILD_TYPE (release, debug) [$BUILD_TYPE]"
    carp
    exit 2
}

hostsetup() {
    if [[ $# -ne 0 ]]; then
        croak 'hostsetup does not take any arguments'
    fi
    set -xe
    mkdir -p "$host_source_dir"
    pushd "$host_source_dir"
    cmake \
        -DBUILD_TYPE=release \
        -DCMAKE_INSTALL_PREFIX="$host_prefix_dir" \
        -DQT_VERSION="$QT_VERSION" \
        -DMULTIMEDIA=OFF \
        -DSHADERTOOLS=ON \
        -DSVG=OFF \
        -DIMAGEFORMATS=OFF \
        -DTRANSLATIONS=OFF \
        -DWEBSOCKETS=OFF \
        -P "$SRC_DIR/.github/scripts/build-qt.cmake"
    popd
    set +xe
    echo
    echo "*** Host setup complete. ***"
    echo
    echo "To set up an Emscripten build now, run:"
    echo "    $0 emsetup"
    echo
}

check_dependencies() {
    local error version
    error=0

    version="$(emcc --version)"
    if [[ $? -ne 0 ]]; then
        error=1
        carp "Error: Could not run emcc. You must install Emscripten " \
            "version $REQUIRED_EMSCRIPTEN_VERSION and then activate it in" \
            "this shell to continue."
    elif ! grep -qP "\\b$REQUIRED_EMSCRIPTEN_VERSION\\b" <<<"$version"; then
        error=1
        carp "Error: The current Emscripten doesn't seem to match the " \
            "required version '$REQUIRED_EMSCRIPTEN_VERSION'. Install and " \
            "activate it in this shell to continue."
    fi

    if ! command -v ninja &> /dev/null; then
        error=1
        carp "Error: 'ninja' not available. Install ninja-build to continue."
    fi

    if ! command -v npm &> /dev/null; then
        error=1
        carp "Error: 'npm' not available. Install nodejs to continue."
    fi

    if [[ $error -ne 0 ]]; then
        croak 'Errors encountered, bailing out.'
    fi
}

run_build_script() {
    set -xe
    mkdir -p "$build_source_dir"
    pushd "$build_source_dir"
    cmake \
        -DBUILD_TYPE="$cmake_build_type" \
        -DCMAKE_PREFIX_PATH="$build_prefix_dir" \
        -DCMAKE_INSTALL_PREFIX="$build_prefix_dir" \
        -DEMSCRIPTEN=ON \
        -DEMSCRIPTEN_THREADS=ON \
        -DKEEP_SOURCE_DIRS=ON \
        -DKEEP_BINARY_DIRS=ON \
        "$@"
    popd
    set +xe
}

install_node_modules() {
    set -xe
    pushd "$SCRIPT_DIR/../../src/desktop/wasm"
    npm install --include=dev
    popd
    set +xe
}

build_zlib() {
    set -xe
    mkdir -p "$build_prefix_dir"
    # Make emcc build the zlib port. Needs a dummy compilation.
    emcc -sUSE_ZLIB=1 -o "$build_prefix_dir/port.wasm" "$SCRIPT_DIR/port.c"
    rm "$build_prefix_dir/port.wasm"
    set +xe
}

build_qt() {
    run_build_script \
        -DQT_VERSION="$QT_VERSION" \
        -DEMSCRIPTEN_HOST_PATH="$host_prefix_dir" \
        -P "$SRC_DIR/.github/scripts/build-qt.cmake"
}

build_libwebp() {
    run_build_script \
        -D OVERRIDE_CMAKE_COMMAND="$build_prefix_dir/bin/qt-cmake" \
        -P "$SRC_DIR/.github/scripts/build-ffmpeg.cmake"
}

build_libzip() {
    run_build_script \
        -D OVERRIDE_CMAKE_COMMAND="$build_prefix_dir/bin/qt-cmake" \
        -P "$SRC_DIR/.github/scripts/build-other.cmake"
}

emsetup() {
    if [[ $# -ne 0 ]]; then
        croak 'emsetup does not take any arguments'
    fi
    check_dependencies
    install_node_modules
    build_zlib
    build_qt
    build_libwebp
    build_libzip
    echo
    echo "*** Setup for Emscripten complete. ***"
    echo
    echo "To configure Drawpile now, run:"
    echo "    $0 configure"
    echo
}

configure() {
    check_dependencies
    set -xe
    "$build_prefix_dir/bin/qt-cmake" \
        -DCMAKE_BUILD_TYPE="$cmake_build_type" \
        -DCMAKE_PREFIX_PATH="$build_prefix_dir" \
        -DCMAKE_INSTALL_PREFIX="$install_dir" \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="$cmake_interprocedural_optimization" \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=BOTH \
        -DBUILTINSERVER=OFF \
        -DCLANG_TIDY=OFF \
        -DSERVER=OFF \
        -DTESTS=OFF \
        -DTOOLS=OFF \
        "$@" \
        -G Ninja \
        -S "$SRC_DIR" \
        -B "$build_dir"
    set +xe

    echo
    echo "*** Configuration for Emscripten complete. ***"
    echo
    echo "To build Drawpile now, run:"
    echo "    cmake --build $build_dir"
    echo
}

SCRIPT_DIR="$(readlink -f "$(dirname "$0")")"
if [[ -z "$SCRIPT_DIR" ]]; then
    croak "Could not figure out script directory of '$0'"
fi

: "${BUILD_TYPE:=release}"

case $BUILD_TYPE in
    'debug')
        cmake_build_type=Debug
        cmake_interprocedural_optimization=OFF
        ;;
    'release')
        cmake_build_type=Release
        cmake_interprocedural_optimization=ON
        ;;
    *)
        croak "Unknown BUILD_TYPE '$BUILD_TYPE'"
        ;;
esac

build_dir="buildem$BUILD_TYPE"
install_dir="installem$BUILD_TYPE"
build_source_dir="$SCRIPT_DIR/build$BUILD_TYPE/source"
build_prefix_dir="$SCRIPT_DIR/build$BUILD_TYPE/prefix"
host_source_dir="$SCRIPT_DIR/buildhost/source"
host_prefix_dir="$SCRIPT_DIR/buildhost/prefix"

SRC_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
if [[ -z $SRC_DIR ]]; then
    croak "Couldn't figure out source directory based on '$SCRIPT_DIR'"
fi

case $1 in
    'hostsetup')
        hostsetup "${@:2}"
        ;;
    'emsetup')
        emsetup "${@:2}"
        ;;
    'configure')
        configure "${@:2}"
        ;;
    *)
        croak_with_usage
        ;;
esac
