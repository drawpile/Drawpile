# SPDX-License-Identifier: MIT

dp_add_library(dpimpex)
dp_target_sources(dpimpex
    dpimpex/image_impex.c
    dpimpex/image_impex.h
    dpimpex/image_jpeg.h
    dpimpex/image_png.h
    dpimpex/image_webp.c
    dpimpex/image_webp.h
    dpimpex/load.c
    dpimpex/load.h
    dpimpex/load_animation.c
    dpimpex/load_animation.h
    dpimpex/load_psd.cpp
    dpimpex/paint_engine_playback.c
    dpimpex/paint_engine_playback.h
    dpimpex/player_index.c
    dpimpex/player_index.h
    dpimpex/save.c
    dpimpex/save.h
    dpimpex/save_psd.c
    dpimpex/utf16be.h
    dpimpex/utf16be_qt.cpp
    dpimpex/xml_stream.h
    dpimpex/xml_stream_qt.cpp
    dpimpex/zip_archive.h
)

target_include_directories(dpimpex PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dpimpex PUBLIC
    dpengine dpmsg dpcommon dpwebp::dpwebp psd_sdk
    "Qt${QT_VERSION_MAJOR}::Core" "Qt${QT_VERSION_MAJOR}::Xml"
)

if(ZIP_IMPL STREQUAL "LIBZIP")
    target_sources(dpimpex PRIVATE dpimpex/zip_archive_libzip.c)
    target_link_libraries(dpimpex PUBLIC libzip::zip)
elseif(ZIP_IMPL STREQUAL "KARCHIVE")
    target_sources(dpimpex PRIVATE dpimpex/zip_archive_karchive.cpp)
    target_link_libraries(dpimpex PUBLIC KF${QT_VERSION_MAJOR}::Archive)
else()
    message(SEND_ERROR "Unknown ZIP_IMPL value '${ZIP_IMPL}'")
endif()

if(IMAGE_IMPL STREQUAL "LIBS")
    target_sources(dpimpex PRIVATE
        dpimpex/image_jpeg.c
        dpimpex/image_png.c
    )
    target_link_libraries(dpimpex PUBLIC
        PNG::PNG
        JPEG::JPEG
    )
elseif(IMAGE_IMPL STREQUAL "QT")
    target_sources(dpimpex PRIVATE dpimpex/image_qt.cpp)
    target_link_libraries(dpimpex PUBLIC "Qt${QT_VERSION_MAJOR}::Gui")
else()
    message(SEND_ERROR "Unknown IMAGE_IMPL value '${IMAGE_IMPL}'")
endif()

if(TARGET LIBAV::LIBAV)
    target_sources(dpimpex PRIVATE
        dpimpex/save_video.c
        dpimpex/save_video.h
    )
    target_link_libraries(dpimpex PUBLIC LIBAV::LIBAV)
    target_compile_definitions(dpimpex PUBLIC DP_LIBAV=1)
endif()

# We only need the WAV reading function on Windows to interact with the
# ridiculous PlaySound API that doesn't support setting a volume.
if(WIN32)
    target_sources(dpimpex PRIVATE
        dpimpex/wav.h
        dpimpex/wav.c
    )
endif()

if(TESTS)
    add_library(dptest_impex)
    target_sources(dptest_impex PRIVATE
        test/dptest/dptest_impex.c
        test/dptest/dptest_impex.h
    )
    target_include_directories(dptest_impex PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/test/dptest"
    )
    target_link_libraries(dptest_impex PUBLIC dptest dpimpex)
    add_dptest_targets(impex dptest_impex
        test/image_thumbnail.c
        test/resize_image.c
    )
endif()
