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
    dpengine/affected_area.c
    dpengine/annotation.c
    dpengine/annotation_list.c
    dpengine/canvas_diff.c
    dpengine/canvas_history.c
    dpengine/canvas_state.c
    dpengine/compress.c
    dpengine/document_metadata.c
    dpengine/draw_context.c
    dpengine/image.c
    dpengine/image_png.c
    dpengine/image_transform.c
    dpengine/layer_content.c
    dpengine/layer_content_list.c
    dpengine/layer_props.c
    dpengine/layer_props_list.c
    dpengine/model_changes.c
    dpengine/ops.c
    dpengine/paint.c
    dpengine/pixels.c
    dpengine/tile.c)

set(dpengine_headers
    dpengine/affected_area.h
    dpengine/annotation.h
    dpengine/annotation_list.h
    dpengine/canvas_diff.h
    dpengine/canvas_history.h
    dpengine/canvas_state.h
    dpengine/compress.h
    dpengine/document_metadata.h
    dpengine/draw_context.h
    dpengine/image.h
    dpengine/image_png.h
    dpengine/image_transform.h
    dpengine/layer_content.h
    dpengine/layer_content_list.h
    dpengine/layer_props.h
    dpengine/layer_props_list.h
    dpengine/model_changes.c
    dpengine/ops.h
    dpengine/paint.h
    dpengine/pixels.h
    dpengine/tile.h)

set(dptest_engine_sources test/dptest/dptest_engine.c)
set(dptest_engine_headers test/dptest/dptest_engine.h)

set(dpengine_tests
    test/handle_annotations.c
    test/handle_metadata.c
    test/image_thumbnail.c
    test/model_changes.c
    test/read_write_image.c
    test/render_recording.c
    test/resize_image.c)

set(dpengine_clang_format_files "${dpengine_sources}" "${dpengine_headers}"
                                "${dptest_engine_sources}"
                                "${dptest_engine_headers}" "${dpengine_tests}"
                                test/handle_metadata.h test/resize_image.h)

add_clang_format_files("${dpengine_clang_format_files}")

add_library(dpengine STATIC "${dpengine_sources}" "${dpengine_headers}")
set_dp_target_properties(dpengine)
target_include_directories(dpengine INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(dpengine PUBLIC dpmsg_interface PNG::PNG qgrayraster)

if(BUILD_TESTS)
    add_library(dptest_engine STATIC "${dptest_engine_sources}"
                                     "${dptest_engine_headers}")
    set_dp_target_properties(dptest_engine NO_EXPORT)
    target_include_directories(dptest_engine INTERFACE
                               "${CMAKE_CURRENT_LIST_DIR}/test/dptest")
    target_link_libraries(dptest_engine PUBLIC dptest dpengine dpmsg)

    add_dptest_targets(engine dptest_engine dpengine_tests)
endif()
