#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -ueo pipefail
set +x

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 VERSION" 1>&2
    exit 2
fi

NEXTVERSION="$1"
export NEXTVERSION

cd "$(dirname "$0")/.."
find Cargo.toml src -name Cargo.toml -exec perl -pi -e 's/^(version\s*=\s*")[^"]+("\s*)$/$1$ENV{NEXTVERSION}$2/g' '{}' ';'

echo
echo "Updated Cargo.tomls to $NEXTVERSION."
echo
echo "Make sure to run a build locally now so that Cargo.lock gets updated!"
echo
