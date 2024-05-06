# SPDX-License-Identifier: MIT
set(INDEX_INPUT_PATH "" CACHE STRING "Input file path for index.html")
set(INDEX_OUTPUT_PATH "" CACHE STRING "Output file path for index.html")
set(SW_INPUT_PATH "" CACHE STRING "Input file path for sw.js")
set(SW_OUTPUT_PATH "" CACHE STRING "Output file path for sw.js")
set(ASSETS_PATH "" CACHE STRING "Asset bundle file path")
set(WASM_PATH "" CACHE STRING "WebAssembly file path")
if(NOT INDEX_INPUT_PATH OR NOT INDEX_OUTPUT_PATH OR NOT SW_INPUT_PATH OR NOT SW_OUTPUT_PATH)
    message(FATAL_ERROR "input and output paths are required")
endif()

find_package(Git)
unset(COMMIT)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --abbrev=999 --always --dirty --exclude=*
        OUTPUT_VARIABLE COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT COMMIT)
    message(WARNING "Could not determine commit version, using '-unknown-'")
    set(COMMIT "-unknown-")
endif()

string(TIMESTAMP CACHEBUSTER "%s" UTC)
file(SIZE "${ASSETS_PATH}" ASSETSIZE)
file(SIZE "${WASM_PATH}" WASMSIZE)
configure_file("${INDEX_INPUT_PATH}" "${INDEX_OUTPUT_PATH}" @ONLY)
configure_file("${SW_INPUT_PATH}" "${SW_OUTPUT_PATH}" @ONLY)
