# SPDX-License-Identifier: MIT

dp_find_package(ZLIB MODULE REQUIRED)

if(NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PKGCONFIG_FOUND)
        pkg_check_modules(LIBAV IMPORTED_TARGET
            libavcodec
            libavformat
            libavutil
            libswscale
        )
        if(TARGET PkgConfig::LIBAV)
            add_library(LIBAV::LIBAV ALIAS PkgConfig::LIBAV)
        endif()
    endif()
endif()
add_feature_info("Video export via libav" "TARGET LIBAV::LIBAV" "")

if(IMAGE_IMPL STREQUAL "LIBS")
    dp_find_package(PNG REQUIRED)
    dp_find_package(JPEG REQUIRED)
elseif(IMAGE_IMPL STREQUAL "QT")
    dp_find_package("Qt${QT_VERSION_MAJOR}" COMPONENTS Gui REQUIRED)
endif()

if(NOT WIN32)
    dp_find_package(Threads REQUIRED)
endif()

dp_find_package("Qt${QT_VERSION_MAJOR}" COMPONENTS Xml REQUIRED)
if(ZIP_IMPL STREQUAL "LIBZIP")
    dp_find_package(libzip QUIET)
    if(NOT TARGET libzip::zip)
        # libzip 1.5 has only pkg-config
        if(PKGCONFIG_FOUND)
            pkg_check_modules(libzip REQUIRED IMPORTED_TARGET libzip)
            add_library(libzip::zip ALIAS PkgConfig::libzip)
        endif()
    endif()
elseif(ZIP_IMPL STREQUAL "KARCHIVE")
    dp_find_package("KF${QT_VERSION_MAJOR}Archive" REQUIRED)
endif()

if(USE_GENERATORS)
    dp_find_package("Qt${QT_VERSION_MAJOR}" COMPONENTS Core Gui)
endif()
