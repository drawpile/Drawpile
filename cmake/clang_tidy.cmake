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

if(USE_CLANG_TIDY)
    find_program(dp_clang_tidy NAMES "clang-tidy")
    if(dp_clang_tidy)
        message(STATUS "Using clang-tidy at ${dp_clang_tidy}")
    else()
        message(STATUS "Did not find clang-tidy, linting disabled")
    endif()
else()
    message(STATUS "Not using clang-tidy, as requested")
endif()

function(add_clang_tidy target)
    if(dp_clang_tidy)
        set_property(GLOBAL APPEND PROPERTY dp_clang_tidy_targets "${target}")
        set_target_properties("${target}" PROPERTIES
            C_CLANG_TIDY "${dp_clang_tidy}")
    endif()
endfunction()

function(report_clang_tidy_targets)
    get_property(targets GLOBAL PROPERTY dp_clang_tidy_targets)
    message(STATUS "Targets with clang-tidy: ${targets}")
endfunction()
