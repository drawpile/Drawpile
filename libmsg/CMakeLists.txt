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
    dpmsg/acl.c
    dpmsg/binary_reader.c
    dpmsg/binary_writer.c
    dpmsg/blend_mode.c
    dpmsg/message.c
    dpmsg/messages.c
    dpmsg/message_queue.c
    dpmsg/msg_internal.c
    dpmsg/text_reader.c
    dpmsg/text_writer.c)

set(dpmsg_headers
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
    dpmsg/text_writer.h)

set(dpmsg_tests
    test/read_write_roundtrip.c)

add_clang_format_files("${dpmsg_sources}" "${dpmsg_headers}" "${dpmsg_tests}")

add_library(dpmsg_interface INTERFACE)
set_dp_target_properties(dpmsg_interface)
target_link_libraries(dpmsg_interface INTERFACE dpcommon)
target_include_directories(dpmsg_interface INTERFACE
                           "${CMAKE_CURRENT_LIST_DIR}")

add_library(dpmsg STATIC "${dpmsg_sources}" "${dpmsg_headers}")
set_dp_target_properties(dpmsg)
target_link_libraries(dpmsg PUBLIC dpmsg_interface parson uthash)

if(LINK_WITH_LIBM)
    target_link_libraries(dpmsg PUBLIC m)
endif()

if(BUILD_TESTS)
    add_dptest_targets(msg dptest dpmsg_tests)
endif()
