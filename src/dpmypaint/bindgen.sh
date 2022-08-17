#!/bin/bash

if [ "$(which bindgen 2>/dev/null)" == "" ]
then
	echo "bindgen not found."
	echo "install it with: cargo install bindgen"
	exit 1
fi

set -e
cd "$(dirname "$0")"
bindgen bindings.h -o src/bindings.rs
