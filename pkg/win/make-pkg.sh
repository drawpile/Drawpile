#!/bin/bash

set -e

MXEROOT=/usr/src/mxe/usr/x86_64-w64-mingw32.shared

if [ ! -d "$MXEROOT" ]; then
	echo "MXE directory not found! Make sure to run this script inside the build container."
	exit 1
fi

if [ ! -d /out ]; then
	echo "Output volume not mounted!"
	exit 1
fi

# Build
/Drawpile/pkg/win/make-build.sh

# Figure out the version number
VERSION=$(readlink /Build/bin/drawpile.exe | cut -d - -f 2)
PKGNAME="drawpile-$VERSION"

cd /out/

trap "chown $HOST_UID -R /out" EXIT

mkdir -p $PKGNAME
rm -f pkg
ln -s $PKGNAME pkg
cd $PKGNAME

# Copy DLLs
MBIN="$MXEROOT/bin"
cp "$MBIN/libwinpthread-1.dll" .
cp "$MBIN/libgcc_s_seh-1.dll" .
cp "$MBIN/libstdc++-6.dll" .
cp "$MBIN/libbz2.dll" .
cp "$MBIN/liblzma-5.dll" .
cp "$MBIN/zlib1.dll" .
cp "$MBIN/libpcre2-16-0.dll" .
cp "$MBIN/libpcre-1.dll" .
cp "$MBIN/libharfbuzz-0.dll" .
cp "$MBIN/libpng16-16.dll" .
cp "$MBIN/libjpeg-9.dll" .
cp "$MBIN/libfreetype-6.dll" .
cp "$MBIN/libglib-2.0-0.dll" .
cp "$MBIN/libintl-8.dll" .
cp "$MBIN/libiconv-2.dll" .
cp "$MBIN/libcrypto-1_1-x64.dll" .
cp "$MBIN/libssl-1_1-x64.dll" .
cp "$MBIN/libsqlite3-0.dll" .
cp "$MBIN/libKF5Archive.dll" .
cp "$MBIN/libsodium-23.dll" .
cp "$MBIN/libqt5keychain.dll" .

QROOT="$MXEROOT/qt5"
cp "$QROOT/bin/Qt5Core.dll" .
cp "$QROOT/bin/Qt5Gui.dll" .
cp "$QROOT/bin/Qt5Multimedia.dll" .
cp "$QROOT/bin/Qt5Network.dll" .
cp "$QROOT/bin/Qt5Widgets.dll" .
cp "$QROOT/bin/Qt5Svg.dll" .
cp "$QROOT/bin/Qt5Sql.dll" .

mkdir -p platforms
cp "$QROOT/plugins/platforms/qwindows.dll" platforms/

mkdir -p styles
cp "$QROOT/plugins/styles/qwindowsvistastyle.dll" styles/

mkdir -p audio
cp "$QROOT/plugins/audio/qtaudio_windows.dll" audio/

mkdir -p iconengines
cp "$QROOT/plugins/iconengines/qsvgicon.dll" iconengines/

mkdir -p sqldrivers
cp "$QROOT/plugins/sqldrivers/qsqlite.dll" sqldrivers/

mkdir -p imageformats
for fmt in qgif qjpeg qsvg
do
	cp "$QROOT/plugins/imageformats/$fmt.dll" imageformats/
done

# Copy Drawpile binaries
cp "$(readlink -f /Build/bin/drawpile.exe)" drawpile.exe
cp /Build/bin/drawpile-srv.exe .

# Copy Drawpile resources
cp -r /Drawpile/desktop/theme .
cp -r /Drawpile/desktop/sounds .
cp -r /Drawpile/desktop/palettes .
cp /Drawpile/desktop/nightmode.colors .
cp /Drawpile/desktop/kritabright.colors .
cp /Drawpile/desktop/kritadark.colors .
cp /Drawpile/desktop/kritadarker.colors .
cp /Drawpile/desktop/initialbrushpresets.db .

# Copy translations
mkdir -p i18n
for tr in cs de fi fr it ja pt ru uk vi zh
do
	cp "$QROOT/translations/qt_$tr.qm" i18n/ || true
	cp "/Build/src/libclient/drawpile_$tr.qm" i18n/ || true
done

# Copy text files
function toDos {
	sed 's/$'"/`echo \\\r`/" $1 > $2
}

toDos /Drawpile/ChangeLog Changelog.txt
toDos /Drawpile/README.md Readme.txt
toDos /Drawpile/AUTHORS Authors.txt
toDos /Drawpile/COPYING License.txt

# Make a ZIP package
cd /out
zip -r -9 $PKGNAME.zip $PKGNAME

