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

target_include_directories(dpmsg PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dpmsg PUBLIC dpcommon uthash parson)

if(TESTS)
    add_dptest_targets(msg dptest
        test/read_write_roundtrip.c
    )
endif()
