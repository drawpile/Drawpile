# SPDX-License-Identifier: MIT

dp_add_library(dpengine)
dp_target_sources(dpengine
    dpengine/affected_area.c
    dpengine/annotation.c
    dpengine/annotation_list.c
    dpengine/brush.c
    dpengine/brush_engine.c
    dpengine/brush_preview.c
    dpengine/canvas_diff.c
    dpengine/canvas_history.c
    dpengine/canvas_state.c
    dpengine/compress.c
    dpengine/dab_cost.c
    dpengine/document_metadata.c
    dpengine/draw_context.c
    dpengine/dump_reader.c
    dpengine/flood_fill.c
    dpengine/image.c
    dpengine/image_transform.c
    dpengine/key_frame.c
    dpengine/layer_content.c
    dpengine/layer_group.c
    dpengine/layer_list.c
    dpengine/layer_props.c
    dpengine/layer_props_list.c
    dpengine/layer_routes.c
    dpengine/load.c
    dpengine/load_animation.c
    dpengine/load_psd.cpp
    dpengine/local_state.c
    dpengine/ops.c
    dpengine/paint.c
    dpengine/paint_engine.c
    dpengine/pixels.c
    dpengine/player.c
    dpengine/preview.c
    dpengine/recorder.c
    dpengine/renderer.c
    dpengine/save.c
    dpengine/save_psd.c
    dpengine/selection.c
    dpengine/selection_set.c
    dpengine/snapshots.c
    dpengine/text.c
    dpengine/tile.c
    dpengine/tile_iterator.c
    dpengine/timeline.c
    dpengine/track.c
    dpengine/user_cursors.c
    dpengine/utf16be_qt.cpp
    dpengine/view_mode.c
    dpengine/xml_stream_qt.cpp
    dpengine/affected_area.h
    dpengine/annotation.h
    dpengine/annotation_list.h
    dpengine/brush.h
    dpengine/brush_engine.h
    dpengine/brush_preview.h
    dpengine/canvas_diff.h
    dpengine/canvas_history.h
    dpengine/canvas_state.h
    dpengine/compress.h
    dpengine/dab_cost.h
    dpengine/document_metadata.h
    dpengine/draw_context.h
    dpengine/dump_reader.h
    dpengine/flood_fill.h
    dpengine/image.h
    dpengine/image_jpeg.h
    dpengine/image_png.h
    dpengine/image_transform.h
    dpengine/key_frame.h
    dpengine/layer_content.h
    dpengine/layer_group.h
    dpengine/layer_list.h
    dpengine/layer_props.h
    dpengine/layer_props_list.h
    dpengine/layer_routes.h
    dpengine/local_state.h
    dpengine/ops.h
    dpengine/paint.h
    dpengine/paint_engine.h
    dpengine/pixels.h
    dpengine/player.h
    dpengine/preview.h
    dpengine/recorder.h
    dpengine/renderer.h
    dpengine/selection.h
    dpengine/selection_set.h
    dpengine/snapshots.h
    dpengine/text.h
    dpengine/tile.h
    dpengine/tile_iterator.h
    dpengine/timeline.h
    dpengine/track.h
    dpengine/user_cursors.h
    dpengine/view_mode.h
    dpengine/load.h
    dpengine/load_animation.h
    dpengine/save.h
    dpengine/utf16be.h
    dpengine/xml_stream.h
    dpengine/zip_archive.h
)

target_include_directories(dpengine PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dpengine PUBLIC
    mypaint uthash qgrayraster jo_gifx parson dpmsg dpcommon ZLIB::ZLIB
    psd_sdk "Qt${QT_VERSION_MAJOR}::Core" "Qt${QT_VERSION_MAJOR}::Xml"
)

if(ZIP_IMPL STREQUAL "LIBZIP")
    target_sources(dpengine PRIVATE dpengine/zip_archive_libzip.c)
    target_link_libraries(dpengine PUBLIC libzip::zip)
elseif(ZIP_IMPL STREQUAL "KARCHIVE")
    target_sources(dpengine PRIVATE dpengine/zip_archive_karchive.cpp)
    target_link_libraries(dpengine PUBLIC KF5::Archive)
else()
    message(SEND_ERROR "Unknown ZIP_IMPL value '${ZIP_IMPL}'")
endif()

if(IMAGE_IMPL STREQUAL "LIBS")
    target_sources(dpengine PRIVATE
        dpengine/image_jpeg.c
        dpengine/image_png.c
    )
    target_link_libraries(dpengine PUBLIC
        PNG::PNG
        JPEG::JPEG
    )
elseif(IMAGE_IMPL STREQUAL "QT")
    target_sources(dpengine PRIVATE dpengine/image_qt.cpp)
    target_link_libraries(dpengine PUBLIC "Qt${QT_VERSION_MAJOR}::Gui")
else()
    message(SEND_ERROR "Unknown IMAGE_IMPL value '${IMAGE_IMPL}'")
endif()

if(TESTS)
    add_library(dptest_engine)
    target_sources(dptest_engine PRIVATE
        test/dptest/dptest_engine.c
        test/dptest/dptest_engine.h
    )
    target_include_directories(dptest_engine PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/test/dptest"
    )
    target_link_libraries(dptest_engine PUBLIC dptest dpengine)
    add_dptest_targets(engine dptest_engine
        test/handle_annotations.c
        test/handle_layers.c
        test/handle_metadata.c
        test/handle_timeline.c
        test/image_thumbnail.c
        test/pixel_conversion.c
        test/resize_image.c
    )
endif()

if(BENCHMARKS)
    dp_add_executable(bench_multidab)
    dp_target_sources(bench_multidab bench/bench_multidab.c)
    target_link_libraries(bench_multidab PUBLIC dpengine)
endif()
