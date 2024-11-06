# SPDX-License-Identifier: MIT
if(NOT FILE_DESCRIPTION OR NOT FILE_VERSION OR NOT PRODUCT_NAME OR NOT PRODUCT_VERSION OR NOT SEARCH_PATHS)
    message(FATAL_ERROR "FILE_DESCRIPTION, FILE_VERSION, PRODUCT_NAME, PRODUCT_VERSION and SEARCH_PATHS are required")
endif()

find_program(RCEDIT_COMMAND rcedit REQUIRED)

unset(globs)
foreach(search_path IN LISTS SEARCH_PATHS)
    list(APPEND globs "${search_path}/*.dll" "${search_path}/*.exe")
endforeach()

message(STATUS "Looking for PE files: ${globs}")
file(GLOB_RECURSE pe_paths FOLLOW_SYMLINKS ${globs})
foreach(pe_path IN LISTS pe_paths)
    execute_process(
        COMMAND
            ${RCEDIT_COMMAND}
            "${pe_path}"
            --set-version-string FileDescription "${FILE_DESCRIPTION}"
            --set-version-string FileVersion "${FILE_VERSION}"
            --set-version-string ProductName "${PRODUCT_NAME}"
            --set-version-string ProductVersion "${PRODUCT_VERSION}"
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()
