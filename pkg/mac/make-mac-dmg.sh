#!/bin/bash
echo "NOTE: Run this script while inside of the root Drawpile directory"
echo "ANOTHER NOTE: This script is meant to be run after building drawpile, so do it after :)"
echo "Otherwise the universe might explode into popcorn..."

sleep 2

DRAWPILE="$(pwd)"

# Remove version string from the binary
cd "$DRAWPILE"/build/bin/Drawpile.app/Contents/MacOS
BINFILE="$(readlink -n Drawpile)"
rm Drawpile
mv "$BINFILE" Drawpile

# Package the app in a dmg archive
APPDMG="$(command -v appdmg)"
if [ "$APPDMG" = "/usr/local/bin/appdmg" ]; then
    "$APPDMG" "$DRAWPILE"/pkg/Mac/spec.json "$DRAWPILE"/build/bin/Drawpile.dmg
else
    echo "Ya done goofed, you don't have appdmg installed!; (or maybe it's not in your path, in which case sorry...)"
fi

echo "The Drawpile.dmg archive is in the build directory"