#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Generates C to Rust bindings in Drawdance.
set -ueo pipefail

if ! command -v cbindgen &> /dev/null; then
    echo 1>&2
    echo 'bindgen not found, you probably need to install it first:' 1>&2
    echo '    cargo install --force --locked bindgen-cli' 1>&2
    echo 1>&2
    exit 1
fi

set -x
cd "$(dirname "$0")/../rust"
bindgen \
    -o bindings.rs \
    --rust-target '1.64' \
    --allowlist-function '(DP|json)_.*' \
    --allowlist-type '(DP|JSON)_.*' \
    --allowlist-var 'DP_.*' \
    --no-prepend-enum-name \
    wrapper.h \
    -- \
    -DRUST_BINDGEN=1 \
    -I../bundled \
    -I../libcommon \
    -I../libmsg \
    -I../libengine \
    -I../libimpex
