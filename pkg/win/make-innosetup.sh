#!/bin/bash

set -e

INNOSETUP="C:\\Program Files (x86)\\Inno Setup 5\iscc.exe"

OUTDIR="/$(pwd)/out"
OUTDIR=${OUTDIR//\//\\\\}

echo $OUTDIR
sed "s/OUTDIR/$OUTDIR/g" drawpile.iss > out/drawpile.iss
wine "$INNOSETUP" out/drawpile.iss

