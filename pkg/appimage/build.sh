#!/bin/bash
set -e

if [ $# -ne 1 ]; then
	echo "Usage: ./build.sh <shell|client|server>"
	exit 1
fi

mkdir -p out

cd ../..
IMAGE=drawpile2
SRCVOL="$(pwd):/Drawpile:ro"
OUTVOL="$(pwd)/pkg/appimage/out:/out"

if [ "$1" == "shell" ]; then
	CMD=""
elif [ "$1" == "client" ]; then
	CMD="/Drawpile/pkg/appimage/Build-client"
elif [ "$1" == "server" ]; then
	CMD="/Drawpile/pkg/appimage/Build-server"
else
	echo "Unknown build target: $1"
	exit 1
fi

docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE bash $CMD

