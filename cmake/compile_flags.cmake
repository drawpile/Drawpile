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

    # When LTO is enabled, CMake will try to pass -fno-fat-lto-objects, which in
    # turn causes clang-tidy to spew a warning about not understanding it, which
    # fails the compilation due to -Werror. We have to turn off the ignored
    # optimization argument warning entirely, -Wno-error doesn't work with GCC
    # in turn because it doesn't know that warning and dies because of it. At
    # the time of writing, there's no way to make CMake not pass that flag.
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION AND dp_clang_tidy)
        list(APPEND dp_common_warnings -Wno-ignored-optimization-argument)
    endif()

    if(NOT USE_STRICT_ALIASING)
        list(APPEND dp_common_cflags -fno-strict-aliasing)
    endif()

    set(dp_cflags ${dp_common_cflags})
    set(dp_cxxflags ${dp_common_cflags} -fno-exceptions)
    set(dp_cwarnings ${dp_common_warnings} -Wstrict-prototypes)
    set(dp_cxxwarnings ${dp_common_warnings})
elseif(CMAKE_C_COMPILER MATCHES "MSVC")
    unset(dp_common_cflags)
    set(dp_common_cflags "/utf-8")
    set(dp_cflags ${dp_common_cflags})
    set(dp_cxxflags ${dp_common_cflags})
endif()
