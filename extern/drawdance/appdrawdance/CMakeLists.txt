# SPDX-License-Identifier: MIT

dp_add_executable(drawdance)
target_sources(drawdance PRIVATE
    drawdance/app.c
    drawdance/app.h
    drawdance/canvas_renderer.c
    drawdance/canvas_renderer.h
    drawdance/emproxy.h
    drawdance/gl.c
    drawdance/gl.h
    drawdance/main.c
    drawdance/ui.c
    drawdance/ui.h
    drawdance_gui/gui.cpp
    drawdance_gui/gui.h
    drawdance_lua/client.c
    drawdance_lua/document.c
    drawdance_lua/imgui.cpp
    drawdance_lua/imgui_gen.cpp
    drawdance_lua/lua_bindings.c
    drawdance_lua/lua_bindings.h
    drawdance_lua/lua_util.h
    drawdance_lua/message.c
    drawdance_lua/ui.c
)
target_include_directories(drawdance PRIVATE
    drawdance
    drawdance_gui
    drawdance_lua
    "${PROJECT_BINARY_DIR}/config"
)
target_link_libraries(drawdance PRIVATE
    dpcommon
    dpclient
    dpengine
    dpmsg
    lua
    parson
    imgui
    dpgles2
    SDL2::SDL2
)
target_compile_definitions(drawdance PRIVATE $<$<CONFIG:Debug>:DP_GL_CHECKS>)

if(EMSCRIPTEN)
    target_link_options(drawdance PRIVATE
        "-sPROXY_TO_PTHREAD"
        "-sALLOW_BLOCKING_ON_MAIN_THREAD=0"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createModule"
        "-sDYNAMIC_EXECUTION=0"
        "-sEXPORTED_FUNCTIONS=_main,_DP_send_from_browser"
        "-sEXPORTED_RUNTIME_METHODS=ALLOC_NORMAL,allocate,intArrayFromString"
        "--preload-file=${PROJECT_SOURCE_DIR}/appdrawdance/lua@appdrawdance/lua"
    )
else()
    target_compile_definitions(drawdance PRIVATE DRAWDANCE_IMGUI)
endif()
