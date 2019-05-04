#!/bin/bash

set -e

if [ $# -ne 1 ]; then
	echo "Usage: ./build.sh <shell|pkg|installer|release>"
	exit 1
fi

mkdir out

cd ../..

IMAGE="${IMAGE:-dpwin8}"
SRCVOL="$(pwd):/Drawpile:ro"
OUTVOL="$(pwd)/pkg/win/out:/out"

if [ "$1" == "shell" ]; then
	CMD="bash"
else
	CMD="/Drawpile/pkg/win/make-pkg.sh"
fi

echo "Running $CMD in $IMAGE"

docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL"  $IMAGE $CMD

if [ "$1" == "installer" ] || [ "$1" == "release" ]; then
	cd -
	./make-innosetup.sh
fi

if [ "$1" == "release" ]; then
	cp -v out/drawpile-*setup*.exe ../../desktop/artifacts/
fi

