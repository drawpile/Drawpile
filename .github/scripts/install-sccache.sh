#!/usr/bin/env bash

set -ueo pipefail

script=$0
exit_usage() {
	echo "Error: $*"
	echo ""
	echo "$script <version> <triplet>"
	echo ""
	echo "  version: x.y.z"
	echo "  triplet: cpu-kernel-os"
	exit 1
}

if [[ $# < 2 ]]; then
	exit_usage "Not enough arguments"
fi

mkdir -p .github/sccache/bin
cd .github/sccache/bin
# Use gunzip because some BSD tar does not support -z whereas GNU tar requires
# it but both support gunzip
curl --retry 3 -L \
	"https://github.com/mozilla/sccache/releases/download/v$1/sccache-v$1-$2.tar.gz" \
	| gunzip | tar --strip-components 1 -xf -
