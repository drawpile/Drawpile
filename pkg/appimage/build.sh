#!/bin/bash
set -e

mkdir -p out

cd ../..
IMAGE=drawpile2
SRCVOL="$(pwd):/Drawpile:ro"
OUTVOL="$(pwd)/pkg/appimage/out:/out"

docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE bash /Drawpile/pkg/appimage/Build-client
docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE bash /Drawpile/pkg/appimage/Build-server

