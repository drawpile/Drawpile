#!/bin/bash
set -e

mkdir -p out

cd ..
IMAGE=drawpile2
SRCVOL="$(pwd):/Drawpile:ro"
OUTVOL="$(pwd)/appimage/out:/out"

docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE bash /Drawpile/appimage/Build-client
docker run --rm -ti -v "$SRCVOL" -v "$OUTVOL" $IMAGE bash /Drawpile/appimage/Build-server

