# SPDX-License-Identifier: MIT
# Common beginning for setup.bash and configure.bash.

# Qt strongly suggests using the same Emscripten version they use.
RECOMMENDED_EMSCRIPTEN_VERSION='3.1.50'

carp() {
    echo 1>&2
    echo "$@" 1>&2
    echo 1>&2
}

have_error=0

if [[ $# -eq 1 ]]; then
    build_mode="$1"
    shift
    if [[ $build_mode = debug ]]; then
        build_dir=builddebug
        build_type=debugnoasan
        cmake_build_type=Debug
        cmake_interprocedural_optimization=OFF
        buildem_dir=buildemdebug
        installem_dir=installemdebug
    elif [[ $build_mode = release ]]; then
        build_dir=buildrelease
        build_type=release
        cmake_build_type=Release
        cmake_interprocedural_optimization=ON
        buildem_dir=buildemrelease
        installem_dir=installemrelease
    else
        have_error=1
        carp "Error: Unknown build type '$1', should be 'debug' or 'release'."
    fi
else
    have_error=1
    carp "Error: expected 1 argument, got $#. Usage is: $0 debug|release"
fi

emscripten_version="$(emcc --version)"
if [[ $? -ne 0 ]]; then
    have_error=1
    carp "Error: Could not run emcc. You must install Emscripten (ideally" \
         "version $RECOMMENDED_EMSCRIPTEN_VERSION) and then activate it in" \
         "this shell to continue."
elif ! grep -qP "\\b$RECOMMENDED_EMSCRIPTEN_VERSION\\b" <<<"$emscripten_version"; then
    carp "Warning: The current Emscripten doesn't seem to match the" \
         "recommended version '$RECOMMENDED_EMSCRIPTEN_VERSION'. Will" \
         "continue anyway, but you may run into incompatibilities."
fi

SRC_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
if [[ -z $SRC_DIR ]]; then
    have_error=1
    carp "Couldn't figure out source directory based on $SCRIPT_DIR"
fi

if [[ $have_error -ne 0 ]]; then
    carp 'Errors encountered, bailing out.'
    exit 1
fi
