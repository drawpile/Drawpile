# SPDX-License-Identifier: MIT

if(USE_GENERATORS)
    add_custom_command(
        OUTPUT "${PROJECT_SOURCE_DIR}/libcommon/dpcommon/conversions.h"
        COMMAND generate_conversions
        ARGS "${PROJECT_SOURCE_DIR}/libcommon/dpcommon/conversions.h")
endif()

dp_add_library(dpcommon)
dp_target_sources(dpcommon
    dpcommon/atomic.c
    dpcommon/base64.c
    dpcommon/binary.c
    dpcommon/common.c
    dpcommon/cpu.c
    dpcommon/endianness.c
    dpcommon/event_log.c
    dpcommon/file.c
    dpcommon/input.c
    dpcommon/memory_pool.c
    dpcommon/output.c
    dpcommon/perf.c
    dpcommon/queue.c
    dpcommon/threading_common.c
    dpcommon/vector.c
    dpcommon/worker.c
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
    dpcommon/memory_pool.h
    dpcommon/output.h
    dpcommon/perf.h
    dpcommon/queue.h
    dpcommon/threading.h
    dpcommon/vector.h
    dpcommon/worker.h
)

target_include_directories(dpcommon PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dpcommon PUBLIC ZLIB::ZLIB)

if(WIN32)
    target_sources(dpcommon PRIVATE dpcommon/threading_win32.c)
elseif(APPLE)
    target_sources(dpcommon PRIVATE dpcommon/threading_qt.cpp)
    target_link_libraries(dpcommon PUBLIC Qt${QT_VERSION_MAJOR}::Core)
else()
    target_sources(dpcommon PRIVATE dpcommon/threading_pthread.c)
    target_link_libraries(dpcommon PUBLIC Threads::Threads)
endif()

include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)

if(IS_BIG_ENDIAN)
    target_compile_definitions(dpcommon PUBLIC DP_BYTE_ORDER=2)
else()
    target_compile_definitions(dpcommon PUBLIC DP_BYTE_ORDER=1)
endif()

target_compile_definitions(dpcommon PUBLIC DP_PERF_REALM=$<TARGET_PROPERTY:NAME>)

if(FILE_IO_IMPL STREQUAL "QT")
    target_compile_definitions(dpcommon PUBLIC DP_QT_IO)
    dp_target_sources(dpcommon
        dpcommon/file_qt.cpp
        dpcommon/file_qt.h
        dpcommon/input_qt.cpp
        dpcommon/input_qt.h
        dpcommon/output_qt.cpp
        dpcommon/output_qt.h
    )
    target_link_libraries(dpcommon PUBLIC "Qt${QT_VERSION_MAJOR}::Core")

    if(ZIP_IMPL STREQUAL "KARCHIVE")
        target_compile_definitions(dpcommon PUBLIC DP_QT_IO_KARCHIVE)
        target_link_libraries(dpcommon PUBLIC "KF${QT_VERSION_MAJOR}::Archive")
    endif()
elseif(NOT FILE_IO_IMPL STREQUAL "STDIO")
    message(SEND_ERROR "Unknown FILE_IO_IMPL value '${FILE_IO_IMPL}'")
endif()

if(TESTS)
    add_dptest_targets(common dptest
        test/base64.c
        test/file.c
        test/queue.c
        test/rect.c
        test/vector.c
    )
endif()
