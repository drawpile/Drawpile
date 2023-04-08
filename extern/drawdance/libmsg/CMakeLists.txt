# SPDX-License-Identifier: MIT

dp_add_library(dpmsg)
dp_target_sources(dpmsg
    dpmsg/acl.c
    dpmsg/binary_reader.c
    dpmsg/binary_writer.c
    dpmsg/blend_mode.c
    dpmsg/message.c
    dpmsg/messages.c
    dpmsg/message_queue.c
    dpmsg/msg_internal.c
    dpmsg/text_reader.c
    dpmsg/text_writer.c
    dpmsg/acl.h
    dpmsg/disconnect_reason.h
    dpmsg/binary_reader.h
    dpmsg/binary_writer.h
    dpmsg/blend_mode.h
    dpmsg/message.h
    dpmsg/messages.h
    dpmsg/message_queue.h
    dpmsg/msg_internal.h
    dpmsg/text_reader.h
    dpmsg/text_writer.h
)
target_include_directories(dpmsg ${SUBPROJECT_INCLUDE} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(dpmsg PRIVATE
    dpcommon
    $<BUILD_INTERFACE:uthash>
    $<BUILD_INTERFACE:parson>
)

if(SUBPROJECT)
    # Parent projects that expect to be able to include headers as if the
    # project was installed will expect to be able to
    # `#include <dpmsg/parson/parson.h>`, so we need to fake it
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fakeroot/dpmsg")
    file(CREATE_LINK
        "${PROJECT_SOURCE_DIR}/3rdparty/parson"
        "${CMAKE_CURRENT_BINARY_DIR}/fakeroot/dpmsg/parson"
        SYMBOLIC
    )
    target_include_directories(dpmsg SYSTEM INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/fakeroot>
        # We also need to make sure transitive includes of those headers from
        # this library’s headers will be able to find them
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/3rdparty/parson>
    )
else()
    target_compile_definitions(dpmsg INTERFACE $<INSTALL_INTERFACE:DP_BUNDLED_PARSON>)
    install(
        FILES "${PROJECT_SOURCE_DIR}/3rdparty/parson/parson.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dpmsg/parson"
    )
endif()

if(BUILD_TESTS)
    add_dptest_targets(msg dptest
        test/read_write_roundtrip.c
    )
endif()
