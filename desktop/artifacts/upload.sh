#!/usr/bin/bash

if [ "$1" == "" ]
then
        echo "Usage: $0 version"
        exit 1
fi

function upload() {
	if [ ! -f "$1" ]
	then
		echo "$1 not found!"
		exit 1
	fi
	scp "$1" webfiles@drawpile.net:~/www/$2/
}

upload "Drawpile $1.dmg" osx
upload "drawpile-$1-setup.exe" win
upload "drawpile-$1-setup-w32.exe" win
upload "drawpile-$1.tar.gz" src
upload "../net.drawpile.drawpile.appdata.xml" metadata

