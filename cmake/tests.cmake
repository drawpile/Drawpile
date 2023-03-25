# SPDX-License-Identifier: MIT

enable_testing()

function(add_dptest_targets type lib tests)
    foreach(test_file IN LISTS "${tests}")
        get_filename_component(test_file_name "${test_file}" NAME_WE)
        set(test_name "dp${type}_${test_file_name}_test")

        add_executable("${test_name}" "${test_file}")
        target_include_directories("${test_name}" PRIVATE
            "${CMAKE_SOURCE_DIR}/test/dptest"
        )
        target_link_libraries("${test_name}" PRIVATE "dp${type}" "${lib}")

        add_test(NAME "${test_name}" COMMAND "${test_name}"
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

        set_property(GLOBAL APPEND PROPERTY dptest_targets "${test_name}")
    endforeach()
endfunction()

function(report_dptest_targets)
    get_property(targets GLOBAL PROPERTY dptest_targets)
    list(JOIN targets "\n * " targets)
    message(STATUS "The following tests have been enabled: \n\n * ${targets}\n")
endfunction()
