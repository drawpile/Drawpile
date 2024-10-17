# SPDX-License-Identifier: MIT
if(NOT BUILD_VERSION OR NOT OUTPUT_PATH)
    message(FATAL_ERROR "BUILD_VERSION and OUTPUT_PATH are required")
endif()

message(STATUS "Build version: '${BUILD_VERSION}'")
if(BUILD_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(server "${CMAKE_MATCH_1}")
    set(major "${CMAKE_MATCH_2}")
    set(minor "${CMAKE_MATCH_3}")

    if(BUILD_VERSION MATCHES "-beta\\.([0-9]+)")
        set(beta "${CMAKE_MATCH_1}")
    else()
        set(beta 99)
    endif()

    set(PRODUCT_VERSION "${server}.${major}.${minor}.${beta}")
    message(STATUS "Product version: '${PRODUCT_VERSION}'")
    file(APPEND "${OUTPUT_PATH}" "WINDOWS_PRODUCT_VERSION=${PRODUCT_VERSION}\n")
else()
    message(FATAL_ERROR "Unable to determine product version")
endif()


