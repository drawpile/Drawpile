# SPDX-License-Identifier: MIT
set(INPUT_PATH "" CACHE STRING "Input file path")
set(OUTPUT_PATH "" CACHE STRING "Output file path")
if(NOT INPUT_PATH OR NOT OUTPUT_PATH)
    message(FATAL_ERROR "INPUT_PATH and OUTPUT_PATH are required")
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
configure_file("${INPUT_PATH}" "${OUTPUT_PATH}" @ONLY)
