# SPDX-License-Identifier: MIT

function(dp_add_executable target)
    add_executable(${target})

    install(TARGETS ${target}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endfunction()

function(dp_add_library target)
    add_library(${target})
    add_library(${PROJECT_NAME}::${target} ALIAS ${target})

    set_property(GLOBAL APPEND PROPERTY dp_components "${target}")

    install(TARGETS ${target}
        EXPORT ${target}Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(EXPORT ${target}Targets
        FILE ${PROJECT_NAME}${target}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME}
    )
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
    foreach(file IN LISTS ARGN)
        if(file MATCHES "(\\.[Hh]([Pp][Pp])?|include[\\/][^.]*)$")
            install(
                FILES "${file}"
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${target}"
            )
        endif()
    endforeach()
endfunction()
