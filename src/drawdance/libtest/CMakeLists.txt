# SPDX-License-Identifier: MIT

add_library(dptest)
target_sources(dptest PRIVATE
    dptest/dptest.c
    dptest/dptest.h
)
target_include_directories(dptest INTERFACE
                            "${CMAKE_CURRENT_LIST_DIR}/dptest")
target_link_libraries(dptest PUBLIC dpcommon)

function(add_dptest_targets type lib)
    foreach(test_file IN LISTS ARGN)
        get_filename_component(test_file_name "${test_file}" NAME_WE)
        set(test_name "dp${type}_${test_file_name}_test")

        add_executable("${test_name}" "${test_file}")
        target_include_directories("${test_name}" PRIVATE
            "${PROJECT_SOURCE_DIR}/src/drawdance/libtest/dptest"
        )
        target_link_libraries("${test_name}" PUBLIC "dp${type}" "${lib}")

        add_test(
            NAME "${test_name}"
            COMMAND "${test_name}"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        )
    endforeach()
endfunction()
