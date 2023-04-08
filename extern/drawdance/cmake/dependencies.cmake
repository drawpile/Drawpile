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

if(NOT EMSCRIPTEN)
    dp_find_package("Qt${QT_VERSION_MAJOR}" COMPONENTS Xml REQUIRED)

    if(ZIP_IMPL STREQUAL "LIBZIP")
        dp_find_package(libzip REQUIRED)
    elseif(ZIP_IMPL STREQUAL "KARCHIVE")
        dp_find_package(KF5Archive REQUIRED)
    endif()
endif()

if(BUILD_APPS)
    dp_find_package(SDL2 REQUIRED)
    if(NOT EMSCRIPTEN)
        dp_find_package(CURL REQUIRED COMPONENTS HTTPS HTTP SSL)
    endif()
endif()

if(USE_GENERATORS)
    dp_find_package("Qt${QT_VERSION_MAJOR}" COMPONENTS Core Gui)
endif()
