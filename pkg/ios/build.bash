#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

QT_VERSION_MAJOR=6
QT_VERSION_MINOR=9
QT_VERSION_PATCH=1
QT_VERSION="$QT_VERSION_MAJOR.$QT_VERSION_MINOR.$QT_VERSION_PATCH"

carp() {
    echo "$@" 1>&2
}

croak() {
    echo 1>&2
    carp "$@"
    echo 1>&2
    exit 1
}

croak_with_usage() {
    carp
    carp "Usage:"
    carp "    $0 hostsetup"
    carp "        to set up Qt on this host to be able to cross-compile"
    carp
    carp "The following environment variables can be set:"
    carp "    BUILD_TYPE (release, relwithdebinfo, debug) [$BUILD_TYPE]"
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
    echo "To set up an iOS/iPadOS build now, run:"
    echo "    $0 isetup"
    echo
}

run_build_script() {
    set -xe
    mkdir -p "$build_source_dir"
    pushd "$build_source_dir"
    cmake \
        -DBUILD_TYPE="$deps_build_type" \
        -DCMAKE_PREFIX_PATH="$build_prefix_dir" \
        -DCMAKE_INSTALL_PREFIX="$build_prefix_dir" \
        -DIOS_IPADOS=ON \
        -DTARGET_ARCH=arm64 \
        -DKEEP_SOURCE_DIRS=ON \
        -DKEEP_BINARY_DIRS=ON \
        "$@"
    popd
    set +xe
}

build_qt() {
    run_build_script \
        -DQT_VERSION="$QT_VERSION" \
        -DIOS_IPADOS_HOST_PATH="$host_prefix_dir" \
        -P "$SRC_DIR/.github/scripts/build-qt.cmake"
}

build_ffmpeg() {
    run_build_script \
        -D OVERRIDE_CMAKE_COMMAND="$build_prefix_dir/bin/qt-cmake" \
        -P "$SRC_DIR/.github/scripts/build-ffmpeg.cmake"
}

build_other() {
    run_build_script \
        -D OVERRIDE_CMAKE_COMMAND="$build_prefix_dir/bin/qt-cmake" \
        -P "$SRC_DIR/.github/scripts/build-other.cmake"
}

isetup() {
    if [[ $# -ne 0 ]]; then
            croak 'isetup does not take any arguments'
    fi
    # build_qt
    build_ffmpeg
    build_other
}

configure() {
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
        -DCMAKE_PROGRAM_PATH="$host_prefix_dir/bin" \
        -DMACOSX_BUNDLE_GUI_IDENTIFIER=org.drawpile.gui \
        "$@" \
        -S "$SRC_DIR" \
        -B "$build_dir"
    set +xe
}

SCRIPT_DIR="$(readlink -f "$(dirname "$0")")"
if [[ -z "$SCRIPT_DIR" ]]; then
    croak "Could not figure out script directory of '$0'"
fi

: "${BUILD_TYPE:=debug}"

case $BUILD_TYPE in
    'debug')
        cmake_build_type=Debug
        cmake_interprocedural_optimization=OFF
        deps_build_type=debugnoasan
        ;;
    'relwithdebinfo')
        cmake_build_type=RelWithDebInfo
        cmake_interprocedural_optimization=OFF
        deps_build_type=relwithdebinfo
        ;;
    'release')
        cmake_build_type=Release
        cmake_interprocedural_optimization=ON
        deps_build_type=release
        ;;
    *)
        croak "Unknown BUILD_TYPE '$BUILD_TYPE'"
        ;;
esac

build_dir="buildios$BUILD_TYPE"
install_dir="installios$BUILD_TYPE"
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
    'isetup')
        isetup "${@:2}"
        ;;
    'configure')
        configure "${@:2}"
        ;;
    *)
        croak_with_usage
        ;;
esac
