#!/bin/bash
echo "NOTE: Run this script while inside of the Drawpile/pkg/mac directory"
echo ""
echo "ANOTHER NOTE: This script is meant to be run after building drawpile, so do it after :)"
echo "Otherwise the universe might explode into popcorn..."
APPDMG="$(command -v appdmg)"
if [ "$APPDMG" = ""]
    echo "Ya done goofed, you don't have appdmg installed!"
else
    "$APPDMG" spec.json Drawpile.dmg
fi
