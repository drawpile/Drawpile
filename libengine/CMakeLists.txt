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

set(dpengine_sources
    dpengine/annotation.c
    dpengine/annotation_list.c
    dpengine/blend_mode.c
    dpengine/canvas_diff.c
    dpengine/canvas_history.c
    dpengine/canvas_state.c
    dpengine/compress.c
    dpengine/draw_context.c
    dpengine/image.c
    dpengine/image_png.c
    dpengine/image_transform.c
    dpengine/layer_content.c
    dpengine/layer_content_list.c
    dpengine/layer_props.c
    dpengine/layer_props_list.c
    dpengine/ops.c
    dpengine/paint.c
    dpengine/pixels.c
    dpengine/tile.c)

set(dpengine_headers
    dpengine/annotation.h
    dpengine/annotation_list.h
    dpengine/blend_mode.h
    dpengine/canvas_diff.h
    dpengine/canvas_history.h
    dpengine/canvas_state.h
    dpengine/compress.h
    dpengine/draw_context.h
    dpengine/image.h
    dpengine/image_png.h
    dpengine/image_transform.h
    dpengine/layer_content.h
    dpengine/layer_content_list.h
    dpengine/layer_props.h
    dpengine/layer_props_list.h
    dpengine/ops.h
    dpengine/paint.h
    dpengine/pixels.h
    dpengine/tile.h)

set(dpengine_test_sources test/lib/dpengine_test.c)
set(dpengine_test_headers test/lib/dpengine_test.h)

set(dpengine_tests
    test/handle_annotations.c
    test/render_recording.c
    test/resize_image.c)

set(dpengine_clang_format_files "${dpengine_sources}" "${dpengine_headers}"
                                "${dpengine_test_sources}"
                                "${dpengine_test_headers}" "${dpengine_tests}")

add_clang_format_files("${dpengine_clang_format_files}")

add_library(dpengine STATIC "${dpengine_sources}" "${dpengine_headers}")
set_dp_target_properties(dpengine)
target_include_directories(dpengine INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(dpengine PUBLIC dpmsg_interface PNG::PNG qgrayraster)

if(BUILD_TESTS)
    add_library(dpengine_test STATIC "${dpengine_test_sources}"
                                     "${dpengine_test_headers}")
    set_dp_target_properties(dpengine_test NO_EXPORT)
    target_include_directories(dpengine_test INTERFACE
                               "${CMAKE_CURRENT_LIST_DIR}/test/lib")
    target_link_libraries(dpengine_test PUBLIC dpmsg_test dpengine dpmsg)

    add_dp_test_targets(engine dpengine_tests)
endif()
