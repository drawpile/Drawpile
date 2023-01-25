#!/usr/bin/env bash

set -ueo pipefail

script=$0
exit_usage() {
	echo "Error: $*"
	echo ""
	echo "$script <out_dir> <build_type> <libmicrohttpd_version> <libsodium_version>"
	echo ""
	echo "  out_dir: directory"
	echo "  build_type: 'devel' or 'release'"
	echo "  libmicrohttpd_version: x.y.z"
	echo "  libsodium_version: x.y.z"
	exit 1
}

if [[ $# < 4 ]]; then
	exit_usage "Not enough arguments"
fi

if [[ "$1" == "" ]]; then
	exit_usage "Missing output directory"
fi

libmicrohttpd_flags="--disable-doc --disable-examples --disable-curl"
case "$2" in
	devel)
		echo "Building for devel"
		echo "** THIS ENABLES ASAN **"
		echo "** YOU MAY NEED TO USE '-DCMAKE_EXE_LINKER_FLAGS_INIT=-fsanitize=address' **"
		echo "** TO GENERATE BUILD FILES SUCCESSFULLY **"
		libmicrohttpd_flags="$libmicrohttpd_flags --enable-asserts --enable-sanitizers=address"
	;;
	release)
		echo "Building for release"
	;;
	*)
		exit_usage "Invalid build type"
	;;
esac

if [[ "$3" == "" || "$4" == "" ]]; then
	exit_usage "Missing version"
fi
libmicrohttpd_version=$3
libsodium_version=$4

mkdir -p "$1"
prefix=$(cd "$1" && pwd -P)
build=$(mktemp -d)
cleanup() {
	if [ -d "$build" ]; then
		rmdir "$build"
	fi
}
trap cleanup EXIT

echo "Installing to $prefix"
echo "Build artefacts in $build"

pushd "$(cd "$build" && pwd -P)" > /dev/null

nprocs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo ${NUMBER_OF_PROCESSORS:-2})

export MACOSX_DEPLOYMENT_TARGET=10.11

if [ "${RUNNER_OS-}" == "Windows" ]; then
	msbuild=$(vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\amd64\\MSBuild.exe |head -1)
	mkdir -p "$prefix/bin"
	mkdir -p "$prefix/include"
	mkdir -p "$prefix/lib"

	# libmicrohttpd tries to use xcopy and so the prefix must look like a
	# Windows path
	prefix=$(echo $prefix | sed -e 's/^\/\(.\)\//\1:\\/')

	run_msbuild() {
		"$msbuild" -m "-p:Configuration=$1;Platform=x64;OutDir=$prefix\\$2\\" "$3"
	}
fi

for dep in \
	"libmicrohttpd https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-${libmicrohttpd_version}.tar.gz $libmicrohttpd_flags" \
	"libsodium https://download.libsodium.org/libsodium/releases/libsodium-${libsodium_version}.tar.gz "
do
	read name url flags <<< "$dep"
	mkdir source
	pushd source > /dev/null
	# TODO: Should verify hashes
	# Use gunzip because some BSD tar does not support -z whereas GNU tar
	# requires it but both support gunzip
	curl --retry 3 -L "$url" | gunzip | tar --strip-components 1 -xf -

	if [ "${RUNNER_OS-}" == "Windows" ]; then
		case $name in
			libmicrohttpd)
				run_msbuild Release-dll bin w32/VS-Any-Version/libmicrohttpd.vcxproj
				run_msbuild Release-static lib w32/VS-Any-Version/libmicrohttpd.vcxproj
				rm "$prefix/bin/microhttpd.h"
				mv "$prefix/lib/microhttpd.h" "$prefix/include/microhttpd.h"
			;;
			libsodium)
				run_msbuild DynRelease bin builds/msvc/vs2019/libsodium.sln
				run_msbuild StaticRelease lib builds/msvc/vs2019/libsodium.sln
				mkdir -p "$prefix/include/sodium"
				cp -v src/libsodium/include/sodium.h "$prefix/include"
				cp -v src/libsodium/include/sodium/*.h "$prefix/include/sodium"
			;;
			*)
				echo "Internal error: Unknown component $component"
			exit 1
			;;
		esac
	else
		./configure --prefix "$prefix" $flags
		make install "-j$nprocs"
	fi

	popd > /dev/null
	rm -r source
done

find "$prefix"

popd > /dev/null
