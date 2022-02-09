# Copyright (c) 2022 askmeaboutloom
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.14)

project(drawdance
    VERSION 0.1.0
    DESCRIPTION "A Drawpile Client"
    LANGUAGES C CXX)

option(USE_GENERATORS "Do code generation (turn off when cross-compiling)" ON)
option(USE_ADDRESS_SANITIZER "Build with address sanitizer enabled" ON)
option(USE_CLANG_TIDY "Run clang-tidy on build" ON)
option(USE_STRICT_ALIASING "Enable strict aliasing optimizations" OFF)
option(LINK_WITH_LIBM "Link with libm when using math" ON)
option(BUILD_TESTS "Build tests with CMocka" ON)

if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for platform '${CMAKE_SYSTEM_NAME}'")
endif()

if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using toolchain file '${CMAKE_TOOLCHAIN_FILE}'")
endif()

if(BUILD_TESTS)
    enable_testing()
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/FetchCMocka.cmake")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/address_sanitizer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/clang_format.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/clang_tidy.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/compile_flags.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/language_standards.cmake")

function(set_dp_target_properties target)
    set_language_standards("${target}")
    add_address_sanitizer("${target}")

    target_compile_definitions("${target}" PRIVATE "_XOPEN_SOURCE=600")

    if(DRAWDANCE_EMSCRIPTEN)
        target_compile_options("${target}" PRIVATE "-pthread")
        target_link_options("${target}" PRIVATE
                            "-pthread" "SHELL:-s PROXY_TO_PTHREAD"
                            "SHELL:-s ALLOW_BLOCKING_ON_MAIN_THREAD=0"
                            "SHELL:-s ALLOW_MEMORY_GROWTH=1" "SHELL:-s FETCH=1"
                            "SHELL:-s MODULARIZE=1"
                            "SHELL:-s EXPORT_NAME=createModule"
                            "SHELL:-s DYNAMIC_EXECUTION=0")
        get_target_property(target_type "${target}" TYPE)
        if("${target_type}" STREQUAL "EXECUTABLE")
            set_property(TARGET "${target}" PROPERTY SUFFIX ".js")
        endif()
    endif()

    if(NOT "NO_CLANG_TIDY" IN_LIST ARGN)
        add_clang_tidy("${target}")
    endif()

    if(NOT "NO_WARNINGS" IN_LIST ARGN)
        if("CXX" IN_LIST ARGN)
            target_compile_options("${target}" PRIVATE "${dp_cxxflags}")
        else()
            target_compile_options("${target}" PRIVATE "${dp_cflags}")
        endif()
    endif()

    if(NOT USE_STRICT_ALIASING)
        target_compile_definitions("${target}" PRIVATE "DP_NO_STRICT_ALIASING")
    endif()

    string(LENGTH "${PROJECT_SOURCE_DIR}/" project_dir_length)
    target_compile_definitions(
        "${target}" PRIVATE "DP_PROJECT_DIR_LENGTH=${project_dir_length}")
endfunction()

function(add_dp_test_targets type tests)
    foreach(test_file IN LISTS "${tests}")
        get_filename_component(test_file_name "${test_file}" NAME_WE)
        set(test_name "dp${type}_test_${test_file_name}")

        add_executable("${test_name}" "${test_file}")
        set_dp_target_properties("${test_name}")
        target_link_libraries("${test_name}" PUBLIC "dp${type}_test")

        add_test(NAME "${test_name}" COMMAND "${test_name}"
                 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endforeach()
endfunction()

add_subdirectory(3rdparty)
add_subdirectory(generators)
add_subdirectory(libcommon)
add_subdirectory(libmsg)
add_subdirectory(libclient)
add_subdirectory(libengine)
add_subdirectory(appconv)
add_subdirectory(appdrawdance)
define_clang_format_target()

report_address_sanitizer_targets()
report_clang_tidy_targets()
