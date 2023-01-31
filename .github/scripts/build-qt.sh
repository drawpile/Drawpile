#!/usr/bin/env bash

set -ueo pipefail

# In order of prerequisites.
# (multimedia needs shadertools. translations needs tools.)
# svg: for ui icons
# shadertools: required multimedia dependency
# imageformats: for webp imports
# multimedia: for playing sound effects
# tools: for .ts and .ui compilers
# translations: for ui translations
plugins="svg shadertools imageformats multimedia tools translations"
script=$0

exit_usage() {
	echo "Error: $*"
	echo ""
	echo "$script <out_dir> <build_type> <version>"
	echo ""
	echo "  out_dir: directory"
	echo "  build_type: 'devel' or 'release'"
	echo "  version: x.y.z"
	exit 1
}

if [[ $# -lt 3 ]]; then
	exit_usage "Not enough arguments"
fi

if [[ "$1" == "" ]]; then
	exit_usage "Missing output directory"
fi

case "$2" in
	devel) devel=1 ;;
	release) devel=0 ;;
	*) exit_usage "Invalid build type" ;;
esac

if [[ "$3" == "" ]]; then
	exit_usage "Missing version"
fi
version=$3

if [[ $(echo "$version" |cut -d. -f 3) == "" ]]; then
	exit_usage "Incomplete version spec; should be x.y.z"
fi

major=$(echo "$version" |cut -d. -f 1-2)

file_license="opensource-"
qt6=0
case $major in
	6.*)
		min_mac="10.14"
		file_license=
		qt6=1
	;;
	5.1[45])
		min_mac="10.13"
	;;
	5.1[23])
		min_mac="10.12"
	;;
	5.11)
		min_mac="10.11"
	;;
	*)
		# This is a Drawpile-specific requirement
		exit_usage "Version too old; must be at least Qt 5.11"
	;;
esac

nprocs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo ${NUMBER_OF_PROCESSORS:-2})

if [ $devel -eq 1 ]; then
	echo "Building for devel"
	echo "** THIS ENABLES ASAN **"
	echo "** YOU MAY NEED TO USE '-DCMAKE_EXE_LINKER_FLAGS_INIT=-fsanitize=address' **"
	echo "** TO GENERATE BUILD FILES SUCCESSFULLY **"
	if [ $qt6 -eq 1 ]; then
		base_flags="-force-asserts -- -DCMAKE_BUILD_TYPE=RelWithDebInfo"
	else
		base_flags="-force-asserts -release -force-debug-info"
	fi
else
	echo "Building for release"
	if [ $qt6 -eq 1 ]; then
		base_flags=""
	else
		base_flags="-release"
	fi
fi

mkdir -p "$1"
prefix=$(cd "$1" && pwd -P)

if [ $qt6 -eq 1 ]; then
	configure="configure"
	configure_module="$prefix/bin/qt-configure-module"
	make="cmake --build . --parallel $nprocs"
	install="cmake --install ."
	base_flags="$base_flags -DCMAKE_OSX_DEPLOYMENT_TARGET=$min_mac"
	tools_flags="-no-feature-assistant -no-feature-pixeltool"
	if [ "${RUNNER_OS-}" == "Windows" ]; then
		configure_module="cmd $configure_module.bat"
	fi
else
	configure="configure"
	configure_module="$prefix/bin/qmake"
	make="make -j $nprocs"
	install="make install"
	base_flags="$base_flags -opensource -confirm-license QMAKE_MACOSX_DEPLOYMENT_TARGET=$min_mac"
	# Disabling assistant in Qt5 breaks the build
	tools_flags="-- -no-feature-pixeltool"

	if [ "${RUNNER_OS-}" == "Windows" ]; then
		# qmake tries to use the wrong link.exe if this at the start of the path,
		# which it is for some weird reason (it is also in the path again later)
		export PATH=${PATH/\/usr\/bin:/}
		# qmake build fails if the non-Windows configure is used
		configure="configure.bat"
		make=$(which jom 2>/dev/null || echo nmake)
		install="$make install"
	fi
fi

build=$(mktemp -d)
cleanup() {
	if [ -d "$build" ]; then
		rmdir "$build"
	fi
}
trap cleanup EXIT

echo "Installing $version to $prefix"
echo "Build artefacts in $build"

pushd "$(cd "$build" && pwd -P)" > /dev/null

# Doing this with the submodules because GitHub Actions instances have 14GiB
# of space and the monolith Qt source code is ~3GiB and trying to build and
# cache it all via e.g. vcpkg causes the Windows VM to run out of disk space.
# Slightly more complex, extremely less slow and wasteful.
for component in base $plugins; do
	if [[ "$component" == "shadertools" && $qt6 == 0 ]]; then
		continue
	fi

	echo "Building qt$component"

	mkdir source
	pushd source > /dev/null
	# TODO: Should verify hashes
	# Use unxz because some BSD tar does not support -J whereas GNU tar requires
	# it but both support unxz
	curl --retry 3 -L "https://download.qt.io/archive/qt/$major/$version/submodules/qt$component-everywhere-${file_license}src-$version.tar.xz" | unxz | tar --strip-components 1 -xf -
	popd > /dev/null

	mkdir build
	pushd build > /dev/null
	case $component in
	base)
		../source/$configure -prefix "$prefix" $base_flags
	;;
	imageformats|multimedia|shadertools|svg|translations)
		$configure_module ../source
	;;
	tools)
		$configure_module ../source $tools_flags
	;;
	*)
		echo "Internal error: Unknown component $component"
		exit 1
	;;
	esac
	$make
	# Some install targets in Qt5 have broken dependency chains so
	# e.g. libqtpcre2.a will not be built before it tries to get installed
	# if we only use the install target
	$install
	popd > /dev/null

	rm -r source build
done

popd > /dev/null
