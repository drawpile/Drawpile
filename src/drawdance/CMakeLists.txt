# SPDX-License-Identifier: MIT

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

include(DrawdanceCompilerOptions)
include(DrawdanceHelpers)
include(DrawdanceOptions)
include(DrawdanceDependencies)

if(TESTS)
    add_subdirectory(libtest)
endif()

add_subdirectory(bundled)
add_subdirectory(generators)
add_subdirectory(libcommon)
add_subdirectory(libmsg)
add_subdirectory(libengine)

add_library(drawdance_server INTERFACE)
target_link_libraries(
    drawdance_server INTERFACE dpengine dpmsg dpcommon cmake-config)

if(CLIENT OR TOOLS)
    add_subdirectory(libimpex)
    add_library(drawdance_client INTERFACE)
    target_link_libraries(
        drawdance_client INTERFACE dpimpex dpengine dpmsg dpcommon cmake-config)
endif()
