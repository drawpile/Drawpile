# SPDX-License-Identifier: MIT

string(LENGTH "${PROJECT_SOURCE_DIR}/" project_dir_length)
add_compile_definitions(
    "DP_PROJECT_DIR_LENGTH=${project_dir_length}"
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
