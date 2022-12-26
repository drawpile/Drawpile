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
    dpcommon/input.c
    dpcommon/output.c
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
    dpcommon/endianness.h
    dpcommon/geom.h
    dpcommon/input.h
    dpcommon/output.h
    dpcommon/queue.h
    dpcommon/threading.h
    dpcommon/vector.h
    dpcommon/worker.h)

set(dpcommon_tests
    test/base64.c
    test/queue.c
    test/rect.c
    test/vector.c)

add_clang_format_files("${dpcommon_sources}" "${dpcommon_headers}"
                       "${dpcommon_tests}" dpcommon/threading_pthread.c
                       dpcommon/threading_qt.cpp dpcommon/threading_sdl.c)

if("${THREAD_IMPL}" STREQUAL "PTHREAD")
    list(APPEND dpcommon_sources dpcommon/threading_pthread.c)
elseif("${THREAD_IMPL}" STREQUAL "QT")
    add_library(dpcommon_qt_threads STATIC dpcommon/threading_qt.cpp)
    set_dp_target_properties(dpcommon_qt_threads CXX)
    target_link_libraries(dpcommon_qt_threads PUBLIC Qt5::Core)
elseif("${THREAD_IMPL}" STREQUAL "SDL")
    list(APPEND dpcommon_sources dpcommon/threading_sdl.c)
else()
    message(SEND_ERROR "Unknown THREAD_IMPL '${THREAD_IMPL}'")
endif()

add_library(dpcommon STATIC "${dpcommon_sources}" "${dpcommon_headers}")
set_dp_target_properties(dpcommon)
target_include_directories(dpcommon INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

if("${THREAD_IMPL}" STREQUAL "PTHREAD")
    target_compile_definitions(dpcommon PUBLIC DRAWDANCE_THREADING_PTHREAD)
    target_link_libraries(dpcommon PUBLIC Threads::Threads)
elseif("${THREAD_IMPL}" STREQUAL "QT")
    target_compile_definitions(dpcommon PUBLIC DRAWDANCE_THREADING_QT)
    target_link_libraries(dpcommon PUBLIC dpcommon_qt_threads)
elseif("${THREAD_IMPL}" STREQUAL "SDL")
    target_compile_definitions(dpcommon PUBLIC DRAWDANCE_THREADING_SDL)
    target_link_libraries(dpcommon PUBLIC SDL2::SDL2)
endif()

include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    target_compile_definitions(dpcommon PUBLIC DP_BYTE_ORDER=2)
else()
    target_compile_definitions(dpcommon PUBLIC DP_BYTE_ORDER=1)
endif()

if(BUILD_TESTS)
    add_dptest_targets(common dptest dpcommon_tests)
endif()
