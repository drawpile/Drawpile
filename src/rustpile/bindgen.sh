#!/bin/bash

if [ "$(which cbindgen 2>/dev/null)" == "" ]
then
	echo "cbindgen not found."
	echo "install it with: cargo install cbindgen"
	exit 1
fi

cbindgen --config cbindgen.toml --lang c++ --crate rustpile --output rustpile.h

