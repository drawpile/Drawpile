#!/bin/bash
# SPDX-License-Identifier: MIT
# Downloads all relevant Qt submodules, checks the hashes and then hashes them
# all for inclusion into build-qt.cmake.
set -e

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 VERSION" 1>&2
    exit 2
fi

version="$1"
major="$(echo "$version" | perl -pe 's/\.[0-9]+\.[0-9]+$//')"
minor="$(echo "$version" | perl -pe 's/^[0-9]+\.//; s/\.[0-9]+$//')"

declare -a modules=(
    qtbase
    qtsvg
    qtimageformats
    qtmultimedia
    qttools
    qttranslations
    qtwebsockets
)

if [[ $major = 5 ]]; then
    modules+=(qtandroidextras)
    suffix=-opensource
elif [[ $major = 6 ]]; then
    modules+=(qtshadertools)
    suffix=
else
    echo "Unhandled Qt major version $major in $version" 1>&2
    exit 1
fi

mkdir "qt$version"
cd "qt$version"

for module in "${modules[@]}"; do
    wget "https://download.qt.io/archive/qt/$major.$minor/$version/submodules/$module-everywhere$suffix-src-$version.tar.xz"
done

wget "https://download.qt.io/archive/qt/$major.$minor/$version/submodules/md5sums.txt"
md5sum -c --ignore-missing md5sums.txt

sha384sum *.tar.xz
