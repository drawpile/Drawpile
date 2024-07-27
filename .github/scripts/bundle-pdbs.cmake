# SPDX-License-Identifier: MIT
if(NOT DLL_SEARCH_PATHS OR NOT EXE_SEARCH_PATHS)
    message(FATAL_ERROR "DLL_SEARCH_PATHS and EXE_SEARCH_PATHS are required")
endif()

message(STATUS "DLL search paths: '${EXE_SEARCH_PATHS}'")
message(STATUS "EXE search paths: '${DLL_SEARCH_PATHS}'")

message(STATUS "Figuring out version from packages")
file(GLOB_RECURSE package_paths FOLLOW_SYMLINKS "Drawpile-*.msi" "Drawpile-*.zip")
message(STATUS "Found package files: '${package_paths}'")
if(NOT package_paths)
    message(FATAL_ERROR "No package file found")
endif()

list(GET package_paths 0 package_path)
message(STATUS "Using package path: '${package_path}'")
get_filename_component(package_base "${package_path}" NAME_WLE)
message(STATUS "Package base: '${package_base}'")

string(REGEX REPLACE "^Drawpile-" "DrawpileSymbols-" buffer_path "${package_base}")
message(STATUS "Buffer path: '${buffer_path}'")

set(output_path "${buffer_path}.zip")
message(STATUS "Output path: '${output_path}'")

unset(symbol_paths)

unset(dll_globs)
foreach(dll_search_path IN LISTS DLL_SEARCH_PATHS)
    list(APPEND dll_globs "${dll_search_path}/*.dll")
endforeach()

message(STATUS "Looking for DLL files: ${dll_globs}")
file(GLOB_RECURSE dll_paths FOLLOW_SYMLINKS ${dll_globs})
foreach(dll_path IN LISTS dll_paths)
    # We don't bundle these libraries, so no need to include their symbols.
    if(NOT "${dll_path}" MATCHES "(Qt[0-9]+(DBus|Designer|DesignerComponents|PrintSupport|Test)|q(direct2d|minimal|offscreen|padlock|xdgdesktopportal)|capi|windowsprintersupport)\\.dll")
        string(REGEX REPLACE "\\.[dD][lL][lL]$" ".pdb" pdb_path "${dll_path}")
        if(EXISTS "${pdb_path}")
            list(APPEND symbol_paths "${pdb_path}")
        endif()
    endif()
endforeach()

unset(exe_globs)
foreach(exe_search_path IN LISTS EXE_SEARCH_PATHS)
    list(APPEND exe_globs "${exe_search_path}/*.exe")
endforeach()

message(STATUS "Looking for EXE files: ${exe_globs}")
file(GLOB_RECURSE exe_paths FOLLOW_SYMLINKS ${exe_globs})
foreach(exe_path IN LISTS exe_paths)
    # We don't care about debug symbols for our test executables.
    if(NOT "${exe_path}" MATCHES "(_test|test_[a-zA-Z0-9_]+)\\.exe$")
        string(REGEX REPLACE "\\.[eE][xX][eE]$" ".pdb" pdb_path "${exe_path}")
        if(EXISTS "${pdb_path}")
            list(APPEND symbol_paths "${pdb_path}")
        endif()
    endif()
endforeach()

if(NOT symbol_paths)
    message(STATUS "No symbol files found")
    return()
endif()

message(STATUS "Removing '${buffer_path}' in case it exists")
file(REMOVE_RECURSE "${buffer_path}")

message(STATUS "Copying symbol files to '${buffer_path}'")
file(COPY ${symbol_paths} DESTINATION "${buffer_path}")

message(STATUS "Compressing '${buffer_path}' to '${output_path}'")
file(ARCHIVE_CREATE OUTPUT "${output_path}" PATHS "${buffer_path}" FORMAT zip)

message(STATUS "Removing '${buffer_path}' afterwards")
file(REMOVE_RECURSE "${buffer_path}")
