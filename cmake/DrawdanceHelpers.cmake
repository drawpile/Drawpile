# SPDX-License-Identifier: MIT

function(dp_add_executable target)
    add_executable(${target})
endfunction()

function(dp_add_library target)
    add_library(${target})
    add_library(${PROJECT_NAME}::${target} ALIAS ${target})
    set_property(GLOBAL APPEND PROPERTY dp_components "${target}")
endfunction()

function(dp_find_package)
    if(EMSCRIPTEN)
        dp_find_package_emscripten(${ARGV})
    else()
        find_package(${ARGV})
    endif()

    list(JOIN ARGV " " dep)
    set_property(GLOBAL APPEND PROPERTY dp_dependencies "find_dependency(${dep})")
endfunction()

function(dp_find_package_emscripten name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "COMPONENTS")

    if(name STREQUAL "Threads")
        add_library(${name}::${name} INTERFACE IMPORTED)
        set_target_properties(${name}::${name} PROPERTIES
            INTERFACE_COMPILE_OPTIONS "-pthread"
            INTERFACE_LINK_LIBRARIES "-pthread"
        )
        return()
    elseif(name STREQUAL "SDL2")
        set(use SDL=2)
    elseif(name STREQUAL "ZLIB")
        set(use ZLIB=1)
    elseif(name STREQUAL "PNG")
        set(use LIBPNG)
    elseif(name STREQUAL "JPEG")
        set(use LIBJPEG=1)
    else()
        find_package(${ARGV})
        return()
    endif()

    foreach(component IN LISTS ARG_COMPONENTS ITEMS ${name})
        add_library(${name}::${component} INTERFACE IMPORTED)
        set_target_properties(${name}::${component} PROPERTIES
            INTERFACE_COMPILE_OPTIONS "-sUSE_${use}"
            INTERFACE_LINK_LIBRARIES "-sUSE_${use}"
        )
    endforeach()
endfunction()

function(dp_target_sources target)
    target_sources(${target} PRIVATE ${ARGN})
endfunction()
