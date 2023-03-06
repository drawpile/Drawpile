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

add_clang_format_files(
    generate_conversions.c
    qt_premul_factors.cpp)


if(USE_GENERATORS)
    add_executable(generate_conversions generate_conversions.c)
    set_dp_target_properties(generate_conversions)

    if("${QT${QT_VERSION}_FOUND}")
        add_executable(qt_image_resize qt_image_resize.cpp)
        set_dp_target_properties(qt_image_resize CXX)
        target_link_libraries(qt_image_resize "Qt${QT_VERSION}::Gui")

        if("${QT_VERSION}" EQUAL 5)
            add_executable(qt_premul_factors qt_premul_factors.cpp)
            set_dp_target_properties(qt_premul_factors CXX)
            target_link_libraries(qt_premul_factors Qt5::Gui)
        else()
            message(STATUS "Not building qt_premul_factors generator since it "
                           "only works with Qt5")
        endif()
    else()
        message(STATUS "Qt${QT_VERSION} not found, not building associated "
                       "generators")
    endif()
endif()
