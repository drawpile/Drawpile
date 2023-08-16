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
