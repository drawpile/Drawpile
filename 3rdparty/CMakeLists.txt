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

if(BUILD_APPS)
    add_library(lua STATIC lua/lapi.c lua/lapi.h lua/lauxlib.c lua/lauxlib.h
                lua/lbaselib.c lua/lcode.c lua/lcode.h lua/lcorolib.c lua/lctype.c
                lua/lctype.h lua/ldblib.c lua/ldebug.c lua/ldebug.h lua/ldo.c
                lua/ldo.h lua/ldump.c lua/lfunc.c lua/lfunc.h lua/lgc.c lua/lgc.h
                lua/linit.c lua/liolib.c lua/ljumptab.h lua/llex.c lua/llex.h
                lua/llimits.h lua/lmathlib.c lua/lmem.c lua/lmem.h lua/loadlib.c
                lua/lobject.c lua/lobject.h lua/lopcodes.c lua/lopcodes.h
                lua/lopnames.h lua/loslib.c lua/lparser.c lua/lparser.h
                lua/lprefix.h lua/lstate.c lua/lstate.h lua/lstring.c lua/lstring.h
                lua/lstrlib.c lua/ltable.c lua/ltable.h lua/ltablib.c lua/ltm.c
                lua/ltm.h lua/lua.h lua/luaconf.h lua/lualib.h lua/lundump.c
                lua/lundump.h lua/lutf8lib.c lua/lvm.c lua/lvm.h lua/lzio.c
                lua/lzio.h )
    set_dp_target_properties(lua NO_CLANG_TIDY NO_WARNINGS)
    target_include_directories(lua INTERFACE "${CMAKE_CURRENT_LIST_DIR}/lua")

    add_clang_format_files(dpgles2/gles2_inc.h)
    add_library(dpgles2 INTERFACE)
    set_dp_target_properties(dpgles2 NO_ASAN NO_CLANG_TIDY NO_WARNINGS)
    target_include_directories(dpgles2 INTERFACE "${CMAKE_CURRENT_LIST_DIR}/dpgles2")

    add_library(imgui STATIC imgui/imconfig.h imgui/imgui.cpp imgui/imgui_demo.cpp
                imgui/imgui_draw.cpp imgui/imgui.h imgui/imgui_impl_opengl3.cpp
                imgui/imgui_impl_opengl3.h imgui/imgui_impl_sdl.cpp
                imgui/imgui_impl_sdl.h imgui/imgui_internal.h imgui/imgui_tables.cpp
                imgui/imgui_widgets.cpp imgui/imstb_rectpack.h imgui/imstb_textedit.h
                imgui/imstb_truetype.h)
    set_dp_target_properties(imgui CXX NO_CLANG_TIDY NO_WARNINGS)
    target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_ES2)
    target_include_directories(imgui INTERFACE "${CMAKE_CURRENT_LIST_DIR}/imgui")
    target_link_libraries(imgui PUBLIC dpgles2 SDL2::SDL2)

    add_library(uriparser STATIC uriparser/UriCommon.c uriparser/UriCommon.h
                uriparser/UriCompare.c uriparser/UriEscape.c uriparser/UriFile.c
                uriparser/UriIp4.c uriparser/UriIp4Base.c uriparser/UriIp4Base.h
                uriparser/UriMemory.c uriparser/UriMemory.h
                uriparser/UriNormalize.c uriparser/UriNormalizeBase.c
                uriparser/UriNormalizeBase.h uriparser/UriParse.c
                uriparser/UriParseBase.c uriparser/UriParseBase.h
                uriparser/UriQuery.c uriparser/UriRecompose.c
                uriparser/UriResolve.c uriparser/UriShorten.c
                uriparser/include/uriparser/Uri.h
                uriparser/include/uriparser/UriBase.h
                uriparser/include/uriparser/UriDefsAnsi.h
                uriparser/include/uriparser/UriDefsConfig.h
                uriparser/include/uriparser/UriIp4.h)
    set_dp_target_properties(uriparser NO_CLANG_TIDY NO_WARNINGS)
    target_compile_definitions(uriparser PUBLIC URI_NO_UNICODE)
    target_include_directories(
            uriparser PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uriparser")
    target_include_directories(
            uriparser PUBLIC "${CMAKE_CURRENT_LIST_DIR}/uriparser/include")
endif()

add_library(parson STATIC parson/parson.c parson/parson.h)
set_dp_target_properties(parson NO_CLANG_TIDY NO_WARNINGS)
target_include_directories(parson INTERFACE "${CMAKE_CURRENT_LIST_DIR}/parson")

add_library(qgrayraster STATIC qgrayraster/qgrayraster.c
                               qgrayraster/qgrayraster_inc.h
                               qgrayraster/qgrayraster_p.h
                               qgrayraster/qrasterdefs_p.h)
set_dp_target_properties(qgrayraster NO_CLANG_TIDY NO_WARNINGS)
target_include_directories(qgrayraster INTERFACE
                           "${CMAKE_CURRENT_LIST_DIR}/qgrayraster")

add_library(mypaint STATIC libmypaint/brushsettings-gen.h
            libmypaint/config.h libmypaint/fastapprox/cast.h
            libmypaint/fastapprox/fasterf.h libmypaint/fastapprox/fastexp.h
            libmypaint/fastapprox/fastgamma.h
            libmypaint/fastapprox/fasthyperbolic.h
            libmypaint/fastapprox/fastlambertw.h libmypaint/fastapprox/fastlog.h
            libmypaint/fastapprox/fastonebigheader.h
            libmypaint/fastapprox/fastpow.h libmypaint/fastapprox/fastsigmoid.h
            libmypaint/fastapprox/fasttrig.h libmypaint/fastapprox/sse.h
            libmypaint/helpers.c libmypaint/helpers.h libmypaint/mypaint-brush.c
            libmypaint/mypaint-brush.h libmypaint/mypaint-brush-settings.c
            libmypaint/mypaint-brush-settings-gen.h
            libmypaint/mypaint-brush-settings.h libmypaint/mypaint.c
            libmypaint/mypaint-config.h libmypaint/mypaint-glib-compat.h
            libmypaint/mypaint.h libmypaint/mypaint-mapping.c
            libmypaint/mypaint-mapping.h libmypaint/mypaint-rectangle.c
            libmypaint/mypaint-rectangle.h libmypaint/mypaint-surface.c
            libmypaint/mypaint-surface.h libmypaint/rng-double.c
            libmypaint/rng-double.h )
set_dp_target_properties(mypaint NO_CLANG_TIDY NO_WARNINGS)
target_include_directories(mypaint INTERFACE
                           "${CMAKE_CURRENT_LIST_DIR}/libmypaint")

add_library(uthash INTERFACE)
set_dp_target_properties(uthash NO_CLANG_TIDY NO_WARNINGS)
target_include_directories(uthash INTERFACE "${CMAKE_CURRENT_LIST_DIR}/uthash")

add_library(jo_gifx STATIC jo_gifx/jo_gifx.h jo_gifx/jo_gifx.c)
set_dp_target_properties(jo_gifx NO_WARNINGS)
target_include_directories(
    jo_gifx INTERFACE "${CMAKE_CURRENT_LIST_DIR}/jo_gifx")
