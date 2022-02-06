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

if(USE_GENERATORS)
    add_custom_command(
            OUTPUT "${CMAKE_SOURCE_DIR}/libcommon/dpcommon/conversions.h"
            COMMAND generate_conversions
            ARGS "${CMAKE_SOURCE_DIR}/libcommon/dpcommon/conversions.h")
endif()

set(dpcommon_sources
    dpcommon/base64.c
    dpcommon/binary.c
    dpcommon/common.c
    dpcommon/input.c
    dpcommon/output.c
    dpcommon/queue.c
    dpcommon/threading.c
    dpcommon/worker.c)

set(dpcommon_headers
    dpcommon/base64.h
    dpcommon/binary.h
    dpcommon/common.h
    dpcommon/conversions.h
    dpcommon/endianness.h
    dpcommon/geom.h
    dpcommon/input.h
    dpcommon/output.h
    dpcommon/queue.h
    dpcommon/threading.h
    dpcommon/worker.h)

set(dpcommon_test_sources test/lib/dpcommon_test.c)
set(dpcommon_test_headers test/lib/dpcommon_test.h)

set(dpcommon_tests
    test/base64_encode.c
    test/queue.c)

add_clang_format_files("${dpcommon_sources}" "${dpcommon_headers}"
                       "${dpcommon_test_sources}" "${dpcommon_test_headers}"
                       "${dpcommon_tests}")

add_library(dpcommon STATIC "${dpcommon_sources}" "${dpcommon_headers}")
set_dp_target_properties(dpcommon)
target_include_directories(dpcommon INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(dpcommon PUBLIC SDL2::SDL2 Threads::Threads)

if(BUILD_TESTS)
    add_library(dpcommon_test STATIC "${dpcommon_test_sources}"
                                     "${dpcommon_test_headers}")
    set_dp_target_properties(dpcommon_test)
    target_include_directories(dpcommon_test INTERFACE
                               "${CMAKE_CURRENT_LIST_DIR}/test/lib")
    target_link_libraries(dpcommon_test PUBLIC cmocka-static dpcommon)

    add_dp_test_targets(common dpcommon_tests)
endif()
