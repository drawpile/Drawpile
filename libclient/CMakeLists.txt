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

set(dpclient_sources
    dpclient/client.c
    dpclient/document.c
    dpclient/uri_utils.c)

set(dpclient_headers
    dpclient/client.h
    dpclient/client_internal.h
    dpclient/document.h
    dpclient/ext_auth.h
    dpclient/uri_utils.h)

add_clang_format_files(
    "${dpclient_sources}" "${dpclient_headers}"
     dpclient/ext_auth.c dpclient/ext_auth_em.c
     dpclient/tcp_socket_client.c dpclient/tcp_socket_client.h
     dpclient/web_socket_client.c dpclient/web_socket_client.h)

if(DRAWDANCE_EMSCRIPTEN)
    list(APPEND dpclient_sources dpclient/web_socket_client.c dpclient/ext_auth_em.c)
    list(APPEND dpclient_headers dpclient/web_socket_client.h)
else()
    list(APPEND dpclient_sources dpclient/tcp_socket_client.c dpclient/ext_auth.c)
    list(APPEND dpclient_headers dpclient/tcp_socket_client.h)
endif()

add_library(dpclient STATIC "${dpclient_sources}" "${dpclient_headers}")
set_dp_target_properties(dpclient)
target_include_directories(dpclient INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(dpclient PUBLIC dpengine SDL2::SDL2 uriparser)

if(DRAWDANCE_EMSCRIPTEN)
    target_link_options(dpclient PUBLIC "-lwebsocket.js")
else()
    target_link_libraries(dpclient PUBLIC CURL::libcurl)
endif()
