#!/usr/bin/env bash

set -ueo pipefail

script=$0
exit_usage() {
	echo "Error: $*"
	echo ""
	echo "$script <path> <version> <triplet>"
	echo ""
	echo "  path: installation path prefix"
	echo "  version: x.y.z"
	echo "  triplet: cpu-kernel-os"
	exit 1
}

if [[ $# < 2 ]]; then
	exit_usage "Not enough arguments"
fi

mkdir -p "$1/bin"
cd "$1/bin"
# Use gunzip because some BSD tar does not support -z whereas GNU tar requires
# it but both support gunzip
curl --retry 3 -L \
	"https://github.com/mozilla/sccache/releases/download/v$2/sccache-v$2-$3.tar.gz" \
	| gunzip | tar --strip-components 1 -xf -
