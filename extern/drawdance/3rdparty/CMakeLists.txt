# SPDX-License-Identifier: MIT

if(BUILD_APPS)
    add_library(lua OBJECT lua/lapi.c lua/lapi.h lua/lauxlib.c lua/lauxlib.h
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
    target_include_directories(lua SYSTEM PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/lua>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    add_library(dpgles2 INTERFACE)
    target_include_directories(dpgles2 SYSTEM INTERFACE "dpgles2")

    add_library(imgui OBJECT imgui/imconfig.h imgui/imgui.cpp imgui/imgui_demo.cpp
                imgui/imgui_draw.cpp imgui/imgui.h imgui/imgui_impl_opengl3.cpp
                imgui/imgui_impl_opengl3.h imgui/imgui_impl_sdl.cpp
                imgui/imgui_impl_sdl.h imgui/imgui_internal.h imgui/imgui_tables.cpp
                imgui/imgui_widgets.cpp imgui/imstb_rectpack.h imgui/imstb_textedit.h
                imgui/imstb_truetype.h)
    target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_ES2)
    target_include_directories(imgui SYSTEM PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/imgui>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(imgui PRIVATE dpgles2 SDL2::SDL2)
    # https://github.com/libsdl-org/SDL/issues/3657
    if(APPLE AND SDL2_VERSION VERSION_LESS 2.0.13)
        target_compile_definitions(imgui PRIVATE _DARWIN_C_SOURCE)
    endif()

    add_library(uriparser OBJECT uriparser/UriCommon.c uriparser/UriCommon.h
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
    target_compile_definitions(uriparser PUBLIC URI_NO_UNICODE)
    target_include_directories(uriparser SYSTEM
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/uriparser/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/uriparser>
    )
endif()

add_library(parson OBJECT parson/parson.c parson/parson.h)
target_include_directories(parson SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/parson>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(qgrayraster OBJECT qgrayraster/qgrayraster.c
                               qgrayraster/qgrayraster_inc.h
                               qgrayraster/qgrayraster_p.h
                               qgrayraster/qrasterdefs_p.h)
target_include_directories(qgrayraster SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/qgrayraster>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(mypaint OBJECT libmypaint/brushsettings-gen.h
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
target_include_directories(mypaint SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/libmypaint>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(uthash INTERFACE)
target_include_directories(uthash INTERFACE "uthash")

add_library(jo_gifx OBJECT jo_gifx/jo_gifx.h jo_gifx/jo_gifx.c)
target_include_directories(jo_gifx SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/jo_gifx>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# The project root adds compiler warnings and tidy to all targets, but
# third-party code does not follow the same code hygiene rules, so turn those
# off for everything in this directory
get_directory_property(third_party_targets BUILDSYSTEM_TARGETS)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
    set_property(TARGET ${third_party_targets} APPEND PROPERTY
        COMPILE_OPTIONS "${IGNORE_WARNINGS_COMPILE_OPTIONS}"
    )
else()
    foreach(target IN LISTS third_party_targets)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "INTERFACE_LIBRARY")
            set(option INTERFACE_COMPILE_OPTIONS)
        else()
            set(option COMPILE_OPTIONS)
        endif()
        set_property(TARGET ${target} APPEND PROPERTY
            ${option} "${IGNORE_WARNINGS_COMPILE_OPTIONS}"
        )
    endforeach()
endif()
# It is not possible to use the same approach that was used to disable
# warnings because CMake apparently discards empty list items when a list is
# passed to set_target_properties, so instead we must loop to unset each
# property
foreach(ignore_tidy IN LISTS IGNORE_TIDY_PROPERTY_NAMES)
    set_property(TARGET ${third_party_targets} PROPERTY ${ignore_tidy})
endforeach()
unset(third_party_targets)
