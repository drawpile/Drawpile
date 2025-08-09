#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

carp() {
    echo "$@" 1>&2
}

croak() {
    carp "$@"
    exit 1
}

croak_with_usage() {
    carp
    carp "Usage:"
    carp "    $0 setup"
    carp "        to set up dependencies"
    carp "    $0 configure"
    carp "        to configure the cmake build of Drawpile"
    carp
    carp "The following environment variables can be set:"
    carp "    BUILD_TYPE (release, debug) [$BUILD_TYPE]"
    carp "    DEPS_BUILD_TYPE (release, debug) [$DEPS_BUILD_TYPE]"
    carp "    QT_VERSION [$QT_VERSION]"
    carp "    DIST_BUILD (ON, OFF) [$DIST_BUILD]"
    carp
    exit 2
}

run_build_script() {
    set -xe
    mkdir -p "$build_source_dir"
    pushd "$build_source_dir"
    cmake \
        -DBUILD_TYPE="$deps_cmake_build_type" \
        -DCMAKE_PREFIX_PATH="$build_prefix_dir" \
        -DCMAKE_INSTALL_PREFIX="$build_prefix_dir" \
        -DTARGET_ARCH="$build_arch" \
        -DKEEP_SOURCE_DIRS=ON \
        -DKEEP_BINARY_DIRS=ON \
        "$@"
    popd
    set +xe
}

build_qt() {
    run_build_script \
        -DQT_VERSION="$QT_VERSION" \
        -P "$SRC_DIR/.github/scripts/build-qt.cmake"
}

build_ffmpeg() {
    run_build_script \
        -P "$SRC_DIR/.github/scripts/build-ffmpeg.cmake"
}

build_other() {
    run_build_script \
        -P "$SRC_DIR/.github/scripts/build-other.cmake"
}

setup() {
    if [[ $# -ne 0 ]]; then
        croak 'setup does not take any arguments'
    fi
    build_qt
    build_ffmpeg
    build_other
    echo
    echo "*** Setup for macOS complete. ***"
    echo
    echo "To configure Drawpile now, run:"
    echo "    $0 configure"
    echo
}

configure() {
    set -xe
    # I don't know why this explicit setting of rpath is necessary, but without
    # it macdeployqt ends up creating an executable with an empty LC_RPATH and
    # then it won't find the Qt framework libraries.
    LDFLAGS='-Wl,-rpath,@executable_path/../Frameworks' cmake \
        -DCMAKE_BUILD_TYPE="$cmake_build_type" \
        -DCMAKE_PREFIX_PATH="$build_prefix_dir" \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="$cmake_interprocedural_optimization" \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=BOTH \
        -DBUILTINSERVER=ON \
        -DCLANG_TIDY=OFF \
        -DSERVER=OFF \
        -DTESTS=OFF \
        -DTOOLS=OFF \
        -DUSE_GENERATORS=OFF \
        -DUSE_STRICT_ALIASING="$use_strict_aliasing" \
        -DDIST_BUILD="$DIST_BUILD" \
        "$@" \
        -G Ninja \
        -S "$SRC_DIR" \
        -B "$build_dir"
    set +xe

    echo
    echo "*** Configuration for macOS complete. ***"
    echo
    echo "To build Drawpile now, run:"
    echo "    $0 build"
    echo
}

build() {
    set -xe
    cmake --build "$build_dir" "$@"
    set +xe

    echo
    echo "*** Build for macOS successful. ***"
    echo
    echo "To package Drawpile into a disk image (.dmg) now, run:"
    echo "    $0 package"
    echo
}

package() {
    set -xe
    cpack --verbose --config "$build_dir/CPackConfig.cmake" "$@"
    set +xe

    echo
    echo "*** Packaging for macOS successful. ***"
    echo
}

SCRIPT_DIR="$(readlink -f "$(dirname "$0")")"
if [[ -z "$SCRIPT_DIR" ]]; then
    croak "Could not figure out script directory of '$0'"
fi

: "${BUILD_TYPE:=release}"
: "${DEPS_BUILD_TYPE:=release}"
: "${QT_VERSION:=5.15.17}"
: "${DIST_BUILD:=ON}"

case $BUILD_TYPE in
    'debug')
        cmake_build_type=Debug
        cmake_interprocedural_optimization=OFF
        use_strict_aliasing=OFF
        ;;
    'release')
        cmake_build_type=Release
        cmake_interprocedural_optimization=ON
        use_strict_aliasing=ON
        ;;
    *)
        croak "Unknown BUILD_TYPE '$BUILD_TYPE'"
        ;;
esac


case $DEPS_BUILD_TYPE in
    'debug')
        deps_cmake_build_type=Debug
        ;;
    'release')
        deps_cmake_build_type=Release
        ;;
    *)
        croak "Unknown DEPS_BUILD_TYPE '$DEPS_BUILD_TYPE'"
        ;;
esac

build_arch="$(uname -m)"
build_dir="buildmacos-$build_arch-$DEPS_BUILD_TYPE-$BUILD_TYPE"
build_source_dir="$SCRIPT_DIR/build$build_arch/$DEPS_BUILD_TYPE/source"
build_prefix_dir="$SCRIPT_DIR/build$build_arch/$DEPS_BUILD_TYPE/prefix"

SRC_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
if [[ -z $SRC_DIR ]]; then
    croak "Couldn't figure out source directory based on '$SCRIPT_DIR'"
fi

case $1 in
    'setup')
        setup "${@:2}"
        ;;
    'configure')
        configure "${@:2}"
        ;;
    'build')
        build "${@:2}"
        ;;
    'package')
        package "${@:2}"
        ;;
    *)
        croak_with_usage
        ;;
esac
