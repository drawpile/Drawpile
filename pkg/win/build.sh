#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: ./build.sh <shell|pkg|installer>"
	exit 1
fi

mkdir -p out

cd ../..

IMAGE="dpwin"
SRCVOL="$(pwd):/Drawpile:ro"
OUTVOL="$(pwd)/pkg/win/out:/out"

if [ "$1" == "shell" ]; then
	CMD="bash"
elif [ "$1" == "pkg" ]; then
	CMD="/Drawpile/pkg/win/make-pkg.sh"
elif [ "$1" == "installer" ]; then
	CMD="/Drawpile/pkg/win/make-installer.sh"
fi

docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE $CMD
