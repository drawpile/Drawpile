# SPDX-License-Identifier: MIT

dp_add_library(dpclient)
dp_target_sources(dpclient
    dpclient/client.c
    dpclient/client.h
    dpclient/document.c
    dpclient/document.h
    dpclient/ext_auth.h
    dpclient/uri_utils.c
    dpclient/uri_utils.h
)
target_sources(dpclient PRIVATE
    dpclient/client_internal.h
)

target_include_directories(dpclient PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(dpclient PRIVATE
    dpcommon
    dpmsg
    dpengine
    SDL2::SDL2
    $<BUILD_INTERFACE:uriparser>
)

if(EMSCRIPTEN)
    dp_target_sources(dpclient
        dpclient/ext_auth_em.c
        dpclient/web_socket_client.c
        dpclient/web_socket_client.h
    )
    target_link_options(dpclient PUBLIC -sFETCH -lwebsocket.js)
else()
    dp_target_sources(dpclient
        dpclient/ext_auth.c
        dpclient/tcp_socket_client.c
        dpclient/tcp_socket_client.h
    )
    target_link_libraries(dpclient PRIVATE CURL::libcurl)
endif()
