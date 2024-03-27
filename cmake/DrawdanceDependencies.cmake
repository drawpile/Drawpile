# SPDX-License-Identifier: MIT

dp_find_package(ZLIB MODULE REQUIRED)

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
        find_package(PkgConfig QUIET)
        if(PKGCONFIG_FOUND)
            pkg_check_modules(libzip REQUIRED IMPORTED_TARGET libzip)
            add_library(libzip::zip ALIAS PkgConfig::libzip)
        endif()
    endif()
elseif(ZIP_IMPL STREQUAL "KARCHIVE")
    dp_find_package(KF5Archive REQUIRED)
endif()

if(USE_GENERATORS)
    dp_find_package("Qt${QT_VERSION_MAJOR}" COMPONENTS Core Gui)
endif()
