#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: ./build.sh <shell|pkg|installer>"
	exit 1
fi

mkdir -p out

cd ../..

IMAGE="${IMAGE:-dpwin8}"
SRCVOL="$(pwd):/Drawpile:ro"
OUTVOL="$(pwd)/pkg/win/out:/out"

if [ "$1" == "shell" ]; then
	CMD="bash"
elif [ "$1" == "pkg" ] || [ "$1" == "installer" ]; then
	CMD="/Drawpile/pkg/win/make-pkg.sh"
fi

echo "Running $CMD in $IMAGE"

docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE $CMD

if [ "$1" == "installer" ]; then
	cd -
	./make-innosetup.sh
fi

