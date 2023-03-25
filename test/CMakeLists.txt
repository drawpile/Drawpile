# SPDX-License-Identifier: MIT

add_library(dptest)
target_sources(dptest PRIVATE
    dptest/dptest.c
    dptest/dptest.h
)
target_include_directories(dptest INTERFACE
                            "${CMAKE_CURRENT_LIST_DIR}/dptest")
target_link_libraries(dptest PRIVATE dpcommon)
