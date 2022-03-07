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

set(dpmsg_sources
    dpmsg/access_tier.c
    dpmsg/binary_reader.c
    dpmsg/binary_writer.c
    dpmsg/message.c
    dpmsg/message_queue.c
    dpmsg/messages/canvas_background.c
    dpmsg/messages/canvas_resize.c
    dpmsg/messages/chat.c
    dpmsg/messages/command.c
    dpmsg/messages/disconnect.c
    dpmsg/messages/draw_dabs.c
    dpmsg/messages/draw_dabs.c
    dpmsg/messages/feature_levels.c
    dpmsg/messages/fill_rect.c
    dpmsg/messages/internal.c
    dpmsg/messages/interval.c
    dpmsg/messages/layer_acl.c
    dpmsg/messages/layer_attr.c
    dpmsg/messages/layer_create.c
    dpmsg/messages/layer_delete.c
    dpmsg/messages/layer_order.c
    dpmsg/messages/layer_retitle.c
    dpmsg/messages/layer_visibility.c
    dpmsg/messages/pen_up.c
    dpmsg/messages/ping.c
    dpmsg/messages/private_chat.c
    dpmsg/messages/put_image.c
    dpmsg/messages/put_tile.c
    dpmsg/messages/region_move.c
    dpmsg/messages/session_owner.c
    dpmsg/messages/soft_reset.c
    dpmsg/messages/trusted_users.c
    dpmsg/messages/user_acl.c
    dpmsg/messages/user_join.c
    dpmsg/messages/user_leave.c
    dpmsg/messages/undo.c
    dpmsg/messages/undo_point.c
    dpmsg/messages/zero_length.c
    dpmsg/text_writer.c)

set(dpmsg_headers
    dpmsg/access_tier.h
    dpmsg/binary_reader.h
    dpmsg/binary_writer.h
    dpmsg/message.h
    dpmsg/message_queue.h
    dpmsg/messages/canvas_background.h
    dpmsg/messages/canvas_resize.h
    dpmsg/messages/chat.h
    dpmsg/messages/command.h
    dpmsg/messages/disconnect.h
    dpmsg/messages/draw_dabs.h
    dpmsg/messages/feature_levels.h
    dpmsg/messages/fill_rect.h
    dpmsg/messages/internal.c
    dpmsg/messages/interval.h
    dpmsg/messages/layer_acl.h
    dpmsg/messages/layer_attr.h
    dpmsg/messages/layer_create.h
    dpmsg/messages/layer_delete.h
    dpmsg/messages/layer_order.h
    dpmsg/messages/layer_retitle.h
    dpmsg/messages/layer_visibility.h
    dpmsg/messages/pen_up.h
    dpmsg/messages/ping.h
    dpmsg/messages/private_chat.h
    dpmsg/messages/put_image.h
    dpmsg/messages/put_tile.h
    dpmsg/messages/region_move.h
    dpmsg/messages/session_owner.h
    dpmsg/messages/soft_reset.h
    dpmsg/messages/trusted_users.h
    dpmsg/messages/user_acl.h
    dpmsg/messages/user_ids.h
    dpmsg/messages/user_join.h
    dpmsg/messages/user_leave.h
    dpmsg/messages/undo.h
    dpmsg/messages/undo_point.h
    dpmsg/messages/zero_length.h
    dpmsg/text_writer.h)

set(dpmsg_test_sources test/lib/dpmsg_test.c)
set(dpmsg_test_headers test/lib/dpmsg_test.h)

set(dpmsg_tests
    test/read_write_roundtrip.c)

add_clang_format_files("${dpmsg_sources}" "${dpmsg_headers}"
                       "${dpmsg_test_sources}" "${dpmsg_test_headers}"
                       "${dpmsg_tests}")

add_library(dpmsg_interface INTERFACE)
target_link_libraries(dpmsg_interface INTERFACE dpcommon)
target_include_directories(dpmsg_interface INTERFACE
                           "${CMAKE_CURRENT_LIST_DIR}")

add_library(dpmsg STATIC "${dpmsg_sources}" "${dpmsg_headers}")
set_dp_target_properties(dpmsg)
target_link_libraries(dpmsg PUBLIC dpmsg_interface parson)

if(LINK_WITH_LIBM)
    target_link_libraries(dpmsg PUBLIC m)
endif()

if(BUILD_TESTS)
    add_library(dpmsg_test STATIC "${dpmsg_test_sources}"
                                  "${dpmsg_test_headers}")
    set_dp_target_properties(dpmsg_test)
    target_include_directories(dpmsg_test INTERFACE
                               "${CMAKE_CURRENT_LIST_DIR}/test/lib")
    target_link_libraries(dpmsg_test PUBLIC dpcommon_test dpmsg)

    add_dp_test_targets(msg dpmsg_tests)
endif()
