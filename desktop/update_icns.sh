#!/bin/sh

# Update the .icns icon bundle for OSX
IS=drawpile.iconset
mkdir $IS
cp drawpile-16x16.png $IS/icon_16x16.png
cp drawpile-32x32.png $IS/icon_32x32.png
cp drawpile-64x64.png $IS/icon_64x64.png
cp drawpile-128x128.png $IS/icon_128x128.png
cp drawpile-256x256.png $IS/icon_256x256.png
iconutil -c icns drawpile.iconset
rm -rf $IS

