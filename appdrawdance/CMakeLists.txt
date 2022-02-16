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

set(drawdance_gui_sources drawdance_gui/gui.cpp)
set(drawdance_gui_headers drawdance_gui/gui.h)

add_clang_format_files("${drawdance_gui_sources}" "${drawdance_gui_headers}")

add_library(drawdance_gui STATIC "${drawdance_gui_sources}"
                                 "${drawdance_gui_headers}")
set_dp_target_properties(drawdance_gui CXX)
target_include_directories(drawdance_gui INTERFACE drawdance_gui)
target_link_libraries(drawdance_gui PUBLIC imgui dpgles2)


set(drawdance_lua_cxx_sources
    drawdance_lua/imgui.cpp
    drawdance_lua/imgui_gen.cpp)

add_clang_format_files("${drawdance_lua_cxx_sources}")

add_library(drawdance_lua_cxx STATIC "${drawdance_lua_cxx_sources}")
set_dp_target_properties(drawdance_lua_cxx CXX)
target_link_libraries(drawdance_lua_cxx PUBLIC dpcommon lua imgui dpgles2)

set(drawdance_lua_sources
    drawdance_lua/canvas_state.c
    drawdance_lua/client.c
    drawdance_lua/document.c
    drawdance_lua/lua_bindings.c
    drawdance_lua/message.c
    drawdance_lua/ui.c)

set(drawdance_lua_headers
    drawdance_lua/lua_bindings.h
    drawdance_lua/lua_util.h)

add_clang_format_files("${drawdance_lua_sources}" "${drawdance_lua_headers}")

add_library(drawdance_lua STATIC "${drawdance_lua_sources}"
                                 "${drawdance_lua_headers}")
set_dp_target_properties(drawdance_lua)
target_include_directories(drawdance_lua PRIVATE "${CMAKE_BINARY_DIR}/config")
target_include_directories(drawdance_lua INTERFACE drawdance_lua)
target_link_libraries(drawdance_lua PUBLIC dpclient lua)

if(NOT DRAWDANCE_EMSCRIPTEN)
    target_compile_definitions(drawdance_lua PRIVATE DRAWDANCE_IMGUI)
    target_link_libraries(drawdance_lua PUBLIC drawdance_lua_cxx)
endif()


set(drawdance_sources
    drawdance/app.c
    drawdance/canvas_renderer.c
    drawdance/gl.c
    drawdance/main.c
    drawdance/ui.c)

set(drawdance_headers
    drawdance/app.h
    drawdance/canvas_renderer.h
    drawdance/emproxy.h
    drawdance/gl.h
    drawdance/ui.h)

add_clang_format_files("${drawdance_sources}" "${drawdance_headers}")

add_executable(drawdance "${drawdance_sources}" "${drawdance_headers}")
set_dp_target_properties(drawdance)
target_link_libraries(drawdance PUBLIC drawdance_lua dpclient dpengine dpgles2)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(drawdance PRIVATE DP_GL_CHECKS)
endif()

if(DRAWDANCE_EMSCRIPTEN)
    target_link_options(drawdance PRIVATE
        "SHELL:-s EXPORTED_FUNCTIONS=_main,_DP_send_from_browser"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=ALLOC_NORMAL,allocate,intArrayFromString"
        "SHELL:--preload-file '${CMAKE_SOURCE_DIR}/appdrawdance/lua@appdrawdance/lua'")
else()
    target_compile_definitions(drawdance PRIVATE DRAWDANCE_IMGUI)
    target_link_libraries(drawdance PUBLIC drawdance_gui)
endif()
