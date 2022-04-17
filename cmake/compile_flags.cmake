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

if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    unset(dp_common_cflags)
    set(dp_common_warnings -pedantic -pedantic-errors -Wall -Wextra -Wshadow
                           -Wmissing-include-dirs -Wconversion -Werror
                           -Wno-error=unused-parameter)

    # MinGW doesn't have %z printf specifiers, so it needs MS-specific formats.
    if(CMAKE_C_COMPILER MATCHES "mingw")
        list(APPEND dp_common_warnings -Wno-pedantic-ms-format)
    endif()

    if(NOT USE_STRICT_ALIASING)
        list(APPEND dp_common_cflags -fno-strict-aliasing)
    endif()

    set(dp_cflags ${dp_common_cflags})
    set(dp_cxxflags ${dp_common_cflags} -fno-exceptions)
    set(dp_cwarnings ${dp_common_warnings} -Wstrict-prototypes)
    set(dp_cxxwarnings ${dp_common_warnings})
endif()
