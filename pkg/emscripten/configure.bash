#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Configures an Emscripten build. Must run setup.bash for it first.

SCRIPT_DIR="$(readlink -f "$(dirname "$0")")"
if [[ -z $SCRIPT_DIR ]]; then
    exit 1
fi
. "$SCRIPT_DIR/common.bash"

set -xe

"$SCRIPT_DIR/$build_dir/emprefix/bin/qt-cmake" \
    "-DCMAKE_BUILD_TYPE=$cmake_build_type" \
    "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=$cmake_interprocedural_optimization" \
    "-DCMAKE_INSTALL_PREFIX=${DP_INSTALL_PREFIX:-"$installem_dir"}" \
    "-DCMAKE_PREFIX_PATH=$SCRIPT_DIR/$build_dir/emprefix" \
    -DBUILTINSERVER=OFF \
    -DCLANG_TIDY=OFF \
    -DTESTS=OFF \
    -G Ninja \
    -S "$SRC_DIR" \
    -B "$buildem_dir"
