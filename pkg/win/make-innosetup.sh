#!/bin/bash

set -e

INNOSETUP="C:\\Program Files (x86)\\Inno Setup 5\iscc.exe"

OUTDIR="/$(pwd)/out"
OUTDIR=${OUTDIR//\//\\\\}

VERSION="$(grep DRAWPILE_VERSION ../../CMakeLists.txt | cut -d \" -f 2)"

sed "s/OUTDIR/$OUTDIR/g;s/DRAWPILE_VERSION/$VERSION/g" drawpile.iss > out/drawpile.iss

wine "$INNOSETUP" out/drawpile.iss

