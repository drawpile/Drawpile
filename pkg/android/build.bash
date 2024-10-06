#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Performs local Android builds. Only tested to work on Linux.

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
    carp "        to set up the Android SDK and dependencies"
    carp "    $0 configure"
    carp "        to configure the cmake build of Drawpile"
    carp
    carp "The following environment variables can be set:"
    carp "    BUILD_TYPE (release, debug) [$BUILD_TYPE]"
    carp "    QT_VERSION [$QT_VERSION]"
    carp "    ANDROID_ABI (armeabi-v7a, arm64-v8a, x86, x86_64) [$ANDROID_ABI]"
    carp "    ANDROID_BUILD_TOOLS_VERSION [$ANDROID_BUILD_TOOLS_VERSION]"
    carp "    ANDROID_NDK_VERSION [$ANDROID_NDK_VERSION]"
    carp "    ANDROID_PLATFORM_VERSION [$ANDROID_PLATFORM_VERSION]"
    carp "    ANDROID_SDK_DIR [$ANDROID_SDK_DIR]"
    carp
    exit 2
}

install_sdk() {
    set -xe
    "$ANDROID_SDKMANAGER" --install \
        "build-tools;$ANDROID_BUILD_TOOLS_VERSION" \
        "ndk;$ANDROID_NDK_VERSION" \
        "platforms;$ANDROID_PLATFORM"
    set +xe
}

check_java_version() {
    java_version="$(java -version 2>&1 | perl -0777 -ne '/version\s+"([0-9]+)\./ && print $1')"
    for expected_version in "$@"; do
        if [[ "$java_version" == "$expected_version" ]]; then
            return 0
        fi
    done
    carp "Java version is $java_version, but for $ANDROID_PLATFORM you need one of: $*"
    return 1
}

check_sdk() {
    local error
    error=0

    if [[ ! -d $ANDROID_SDK_DIR ]]; then
        carp "ANDROID_SDK_DIR is not a directory: '$ANDROID_SDK_DIR'"
        error=1
    fi

    if [[ ! -d $ANDROID_NDK_DIR ]]; then
        carp "ANDROID_NDK_DIR is not a directory: '$ANDROID_NDK_DIR'"
        error=1
    fi

    if [[ ! -f $ANDROID_TOOLCHAIN_FILE ]]; then
        carp "ANDROID_TOOLCHAIN_FILE is not a file: '$ANDROID_TOOLCHAIN_FILE'"
        error=1
    fi

    case $ANDROID_PLATFORM_VERSION in
        '34')
            if ! check_java_version 11 17; then
                error=1
            fi
            ;;
        *)
            carp "Unknown ANDROID_PLATFORM_VERSION '$ANDROID_PLATFORM_VERSION'"
            error=1
            ;;
    esac

    if [[ $error -ne 0 ]]; then
        exit 1
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
        -DANDROID_SDK_ROOT="$ANDROID_SDK_DIR" \
        -DANDROID_NDK_ROOT="$ANDROID_NDK_DIR" \
        -DANDROID_ABI="$ANDROID_ABI" \
        -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
        -DTARGET_ARCH="$TARGET_ARCH" \
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
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN_FILE" \
        -P "$SRC_DIR/.github/scripts/build-ffmpeg.cmake"
}

build_other() {
    run_build_script \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN_FILE" \
        -P "$SRC_DIR/.github/scripts/build-other.cmake"
}

setup() {
    check_sdk
    install_sdk
    build_qt
    build_ffmpeg
    build_other
    echo
    echo "*** Setup for Android complete. ***"
    echo
    echo "To configure Drawpile now, run:"
    echo "    $0 configure"
    echo
}

get_sdk_packages_to_uninstall() {
    "$ANDROID_SDKMANAGER" --list_installed 2>&1 |
        ANDROID_BUILD_TOOLS_VERSION="$ANDROID_BUILD_TOOLS_VERSION" \
        ANDROID_NDK_VERSION="$ANDROID_NDK_VERSION" \
        ANDROID_PLATFORM="$ANDROID_PLATFORM" \
        perl -na \
        -e 'push @p, $F[0] if $F[0] =~ /^(build-tools|ndk|platforms);/ ' \
        -e '&& $F[0] ne "build-tools;$ENV{ANDROID_BUILD_TOOLS_VERSION}" ' \
        -e '&& $F[0] ne "ndk;$ENV{ANDROID_NDK_VERSION}"' \
        -e '&& $F[0] ne "platforms;$ENV{ANDROID_PLATFORM}";' \
        -e 'END { print join " ", @p }'
}

check_sdk_packages() {
    packages_to_uninstall="$(get_sdk_packages_to_uninstall)"
    if [[ -n $packages_to_uninstall ]]; then
        carp
        carp "Found superfluous Android SDK packages installed. Having extra"
        carp "packages present will confuse Qt and break in strange ways."
        carp
        carp "    $packages_to_uninstall"
        carp
        carp "Either move these packages to a different directory or uninstall"
        carp "them by running the following:"
        carp
        carp "    $ANDROID_SDKMANAGER --uninstall $packages_to_uninstall"
        carp
        exit 1
    fi
}

configure() {
    check_sdk
    check_sdk_packages
    set -xe
    cmake \
        -DCMAKE_BUILD_TYPE="$cmake_build_type" \
        -DCMAKE_PREFIX_PATH="$build_prefix_dir" \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="$cmake_interprocedural_optimization" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN_FILE" \
        -DANDROID_ABI="$ANDROID_ABI" \
        -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
        -DANDROID_SDK_BUILD_TOOLS_REVISION="$ANDROID_BUILD_TOOLS_VERSION" \
        -DANDROID_TARGET_SDK_VERSION="$ANDROID_PLATFORM_VERSION" \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=BOTH \
        -DBUILTINSERVER=OFF \
        -DCLANG_TIDY=OFF \
        -DSERVER=OFF \
        -DTESTS=OFF \
        -DTOOLS=OFF \
        -G Ninja \
        -S "$SRC_DIR" \
        -B "$build_dir"
    set +xe

    echo
    echo "*** Configuration for Android complete. ***"
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
: "${QT_VERSION:=5.15.14}"
: "${ANDROID_ABI:=arm64-v8a}"
: "${ANDROID_BUILD_TOOLS_VERSION:=34.0.0-rc3}"
: "${ANDROID_NDK_VERSION:=27.0.12077973}"
: "${ANDROID_PLATFORM_VERSION:=34}"
: "${ANDROID_SDK_DIR:=$HOME/Android/Sdk}"
: "${ANDROID_SDKMANAGER:=$ANDROID_SDK_DIR/cmdline-tools/latest/bin/sdkmanager}"

case $ANDROID_ABI in
    'armeabi-v7a')
        TARGET_ARCH='arm32'
        ;;
    'arm64-v8a')
        TARGET_ARCH='arm64'
        ;;
    'x86')
        TARGET_ARCH='x86'
        ;;
    'x86_64')
        TARGET_ARCH='x86_64'
        ;;
    *)
        croak "Unknown ANDROID_ABI '$ANDROID_ABI'"
        ;;
esac

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

build_dir="buildandroid-$ANDROID_ABI-$BUILD_TYPE"
build_source_dir="$SCRIPT_DIR/build$ANDROID_ABI/$BUILD_TYPE/source"
build_prefix_dir="$SCRIPT_DIR/build$ANDROID_ABI/$BUILD_TYPE/prefix"

SRC_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
if [[ -z $SRC_DIR ]]; then
    croak "Couldn't figure out source directory based on '$SCRIPT_DIR'"
fi

ANDROID_NDK_DIR="$ANDROID_SDK_DIR/ndk/$ANDROID_NDK_VERSION"
ANDROID_TOOLCHAIN_FILE="$ANDROID_NDK_DIR/build/cmake/android.toolchain.cmake"
ANDROID_PLATFORM="android-$ANDROID_PLATFORM_VERSION"

if [[ $# -ne 1 ]]; then
    croak_with_usage
fi

case $1 in
    'setup')
        setup
        ;;
    'configure')
        configure
        ;;
    *)
        croak_with_usage
        ;;
esac
