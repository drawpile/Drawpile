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

if(DRAWDANCE_EMSCRIPTEN)
    add_library(empng INTERFACE)
    add_library(PNG::PNG ALIAS empng)
    target_compile_options(empng INTERFACE "SHELL:-s USE_LIBPNG=1"
                                           "SHELL:-sUSE_ZLIB=1")
    target_link_options(empng INTERFACE "SHELL:-s USE_LIBPNG=1"
                                        "SHELL:-s USE_ZLIB=1")
    add_dp_export_target(empng)

    add_library(emsdl2 INTERFACE)
    add_library(SDL2::SDL2 ALIAS emsdl2)
    target_compile_options(emsdl2 INTERFACE "SHELL:-s USE_SDL=2")
    target_link_options(emsdl2 INTERFACE "SHELL:-s USE_SDL=2")
    target_include_directories(emsdl2 INTERFACE
                               "${CMAKE_SOURCE_DIR}/cmake/emsdl2")
    add_dp_export_target(emsdl2)

    add_library(emthreads INTERFACE)
    add_library(Threads::Threads ALIAS emthreads)
    add_dp_export_target(emthreads)
    # Need to compile and link all targets with -pthreads so that they actually
    # link together in the end, so this target does nothing on Emscripten.
    # The set_dp_target_properties function sets the flag on all targets.

    if(NOT ("${THREAD_IMPL}" STREQUAL "PTHREAD"))
        message(SEND_ERROR "Emscripten supports only THREAD_IMPL PTHREAD")
    endif()
else()
    find_package(ZLIB MODULE REQUIRED)
    find_package(PNG MODULE REQUIRED)
    find_package(JPEG MODULE REQUIRED)

    if("${THREAD_IMPL}" STREQUAL "PTHREAD")
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        set(THREADS_PREFER_PTHREAD_FLAG TRUE)
        find_package(Threads REQUIRED)
    elseif("${THREAD_IMPL}" STREQUAL "QT")
        find_package(Qt5 COMPONENTS Core REQUIRED)
    elseif("${THREAD_IMPL}" STREQUAL "QT6")
        find_package(Qt6 COMPONENTS Core REQUIRED)
    endif()

    if("${XML_IMPL}" STREQUAL "QT")
        find_package(Qt5 COMPONENTS Xml REQUIRED)
    elseif("${XML_IMPL}" STREQUAL "QT6")
        find_package(Qt6 COMPONENTS Xml REQUIRED)
    endif()

    if("${ZIP_IMPL}" STREQUAL "LIBZIP")
        find_package(libzip REQUIRED)
    elseif("${ZIP_IMPL}" STREQUAL "KARCHIVE")
        find_package(KF5Archive REQUIRED)
    endif()

    if(BUILD_APPS)
        find_package(SDL2 REQUIRED)
        find_package(CURL MODULE REQUIRED COMPONENTS HTTPS HTTP SSL)
    endif()

    find_package(Qt5 COMPONENTS Core Gui)
endif()
