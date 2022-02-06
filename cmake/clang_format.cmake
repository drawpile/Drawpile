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

function(add_clang_format_files)
    foreach(path IN LISTS ARGN)
        set_property(GLOBAL APPEND PROPERTY dp_clang_format_files
                     "${CMAKE_CURRENT_LIST_DIR}/${path}")
    endforeach()
endfunction()

function(define_clang_format_target)
    find_program(clang_format NAMES "clang-format")
    if(clang_format)
        message(STATUS "Using clang-format at ${clang_format}")
        message(STATUS "Invoke target 'clang-format' to format code")
        get_property(files GLOBAL PROPERTY dp_clang_format_files)
        add_custom_target(clang-format COMMAND "${clang_format}" -i ${files})
    else()
        message(STATUS "Did not find clang-format")
    endif()
endfunction()
