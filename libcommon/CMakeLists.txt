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
    dpcommon/atomic.c
    dpcommon/base64.c
    dpcommon/binary.c
    dpcommon/common.c
    dpcommon/cpu.c
    dpcommon/event_log.c
    dpcommon/file.c
    dpcommon/input.c
    dpcommon/output.c
    dpcommon/perf.c
    dpcommon/queue.c
    dpcommon/threading_common.c
    dpcommon/vector.c
    dpcommon/worker.c)

set(dpcommon_headers
    dpcommon/atomic.h
    dpcommon/base64.h
    dpcommon/binary.h
    dpcommon/common.h
    dpcommon/conversions.h
    dpcommon/cpu.h
    dpcommon/endianness.h
    dpcommon/event_log.h
    dpcommon/file.h
    dpcommon/geom.h
    dpcommon/input.h
    dpcommon/output.h
    dpcommon/perf.h
    dpcommon/queue.h
    dpcommon/threading.h
    dpcommon/vector.h
    dpcommon/worker.h)

set(dpcommon_tests
    test/base64.c
    test/file.c
    test/queue.c
    test/rect.c
    test/vector.c)

set(dpcommon_qt_io_sources
    dpcommon/input_qt.cpp
    dpcommon/output_qt.cpp)

set(dpcommon_qt_io_headers
    dpcommon/input_qt.h
    dpcommon/output_qt.h)

add_clang_format_files("${dpcommon_sources}" "${dpcommon_headers}"
    "${dpcommon_tests}" "${dpcommon_qt_io_sources}" "${dpcommon_qt_io_headers}"
    dpcommon/threading_pthread.c dpcommon/threading_qt.cpp
    dpcommon/threading_sdl.c)

if(WIN32)
    list(APPEND dpcommon_sources dpcommon/threading_win32.c)
else()
    list(APPEND dpcommon_sources dpcommon/threading_pthread.c)
endif()

add_library(dpcommon STATIC "${dpcommon_sources}" "${dpcommon_headers}")
set_dp_target_properties(dpcommon)
target_include_directories(dpcommon INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(dpcommon PUBLIC ZLIB::ZLIB)

if(NOT WIN32)
    target_link_libraries(dpcommon PUBLIC Threads::Threads)
endif()

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

if(IS_BIG_ENDIAN)
    target_compile_definitions(dpcommon PUBLIC DP_BYTE_ORDER=2)
else()
    target_compile_definitions(dpcommon PUBLIC DP_BYTE_ORDER=1)
endif()

if("${FILE_IO_IMPL}" STREQUAL "STDIO")
    # Nothing to do.
elseif("${FILE_IO_IMPL}" STREQUAL "QT")
    add_library(dpcommon_qt_io STATIC
        "${dpcommon_qt_io_sources}" "${dpcommon_qt_io_headers}")
    set_dp_target_properties(dpcommon_qt_io CXX)
    target_link_libraries(
        dpcommon_qt_io PUBLIC dpcommon "Qt${QT_VERSION}::Core")
    target_compile_definitions(dpcommon PUBLIC DP_QT_IO=1)
    target_link_libraries(dpcommon PUBLIC dpcommon_qt_io)
else()
    message(SEND_ERROR "Unknown FILE_IO_IMPL value '${FILE_IO_IMPL}'")
endif()

if(BUILD_TESTS)
    add_dptest_targets(common dptest dpcommon_tests)
endif()
