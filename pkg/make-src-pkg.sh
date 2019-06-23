#!/bin/bash
# Create a source archive

set -e

if [ "$1" == "" ]
then
	echo "Usage: $0 tag"
	exit 1
fi

cd "$(git rev-parse --show-toplevel)"

OUT="desktop/artifacts/drawpile-$1.tar.gz"

if [ -e "$OUT" ]
then
	echo "File $OUT already exists!"
	exit 1
fi

echo "Creating archive $OUT"
git archive "$1" --prefix="drawpile-$1/" | gzip > "$OUT"

