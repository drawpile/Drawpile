# Copyright (c) 2022 askmeaboutloom
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if(BUILD_TESTS)
    enable_testing()

    function(add_dptest_targets type lib tests)
        foreach(test_file IN LISTS "${tests}")
            get_filename_component(test_file_name "${test_file}" NAME_WE)
            set(test_name "dp${type}_test_${test_file_name}")

            add_executable("${test_name}" "${test_file}")
            set_dp_target_properties("${test_name}" NO_EXPORT)
            target_link_libraries("${test_name}" PUBLIC "dp${type}" "${lib}")

            add_test(NAME "${test_name}" COMMAND "${test_name}"
                    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

            set_property(GLOBAL APPEND PROPERTY dptest_targets "${test_name}")
        endforeach()
    endfunction()
else()
    # Trying to add tests when not enabled is a mistake in the build script.
    function(add_dptest_targets)
        message(FATAL_ERROR "add_dptest_targets called, but BUILD_TESTS is off")
    endfunction()
endif()

function(report_dptest_targets)
    get_property(targets GLOBAL PROPERTY dptest_targets)
    message(STATUS "Test targets: ${targets}")
endfunction()
