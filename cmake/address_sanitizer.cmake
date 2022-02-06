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

if(USE_ADDRESS_SANITIZER)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(dp_asan_options -fno-omit-frame-pointer -fsanitize=address)
        message(STATUS "Using address sanitizer for gcc/clang "
                       "via options '${dp_asan_options}'")
    else()
        message(STATUS "Don't know how to enable address "
                       "sanitizer on ${CMAKE_C_COMPILER_ID}")
    endif()
else()
    message(STATUS "Not using address sanitizer, as requested")
endif()

function(add_address_sanitizer target)
    if(dp_asan_options)
        set_property(GLOBAL APPEND PROPERTY dp_asan_targets "${target}")
        target_compile_definitions("${target}" PRIVATE "DP_ADDRESS_SANITIZER")
        target_compile_options("${target}" PRIVATE ${dp_asan_options})
        target_link_options("${target}" PRIVATE ${dp_asan_options})
    endif()
endfunction()

function(report_address_sanitizer_targets)
    get_property(targets GLOBAL PROPERTY dp_asan_targets)
    message(STATUS "Targets with address sanitizer: ${targets}")
endfunction()
