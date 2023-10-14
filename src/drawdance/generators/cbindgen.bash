#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Generates Rust to C bindings in Drawdance.
set -ueo pipefail

if ! command -v clang-format &> /dev/null; then
    echo 1>&2
    echo "clang-format not found. That's okay, but the generated code" 1>&2
    echo "won't be formatted properly, so don't commit it." 1>&2
    echo 1>&2
fi

if ! command -v cbindgen &> /dev/null; then
    echo 1>&2
    echo 'cbindgen not found, you probably need to install it first:' 1>&2
    echo '    cargo install --force cbindgen' 1>&2
    echo 1>&2
    exit 1
fi


run_cbindgen () (
    set -x
    cd "$1"
    cbindgen --config cbindgen.toml --output "$2"
    clang-format -i "$2"
)

cd "$(dirname "$0")/.."
run_cbindgen libmsg dpmsg/rust.h
run_cbindgen libengine dpengine/rust.h
