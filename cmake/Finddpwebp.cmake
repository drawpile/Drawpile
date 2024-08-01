# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2022 The Qt Company Ltd.
# Altered for Drawpile to rename the target to dpwebp::dpwebp.

# Latest upstream package provides both CMake and autotools building.
# Unfortunately Linux distros and homebrew build the package with autotools,
# so they do not ship the CMake Config file, but only the pkg-config files.
# vcpkg and Conan do ship Config files.
# So try config files first, and then use the regular find_library / find_path dance with pkg-config
# paths as hints.

# We can't create the same interface imported target multiple times, CMake will complain if we do
# that. This can happen if the find_package call is done in multiple different subdirectories.
if(TARGET dpwebp::dpwebp)
    set(dpwebp_FOUND TRUE)
    return()
endif()

find_package(WebP QUIET)
if(TARGET WebP::webp AND TARGET WebP::webpdemux AND TARGET WebP::libwebpmux)
    set(dpwebp_FOUND ON)
    add_library(dpwebp::dpwebp INTERFACE IMPORTED)
    target_link_libraries(dpwebp::dpwebp INTERFACE WebP::webp WebP::webpdemux WebP::libwebpmux)
    return()
endif()

find_package(PkgConfig QUIET)
pkg_check_modules(PC_WebP libwebp)
pkg_check_modules(PC_WebPDemux libwebpdemux)
pkg_check_modules(PC_WebPMux libwebpmux)

find_library(WebP_LIBRARY NAMES "webp"
                          HINTS ${PC_WebP_LIBDIR})
find_library(WebP_demux_LIBRARY NAMES "webpdemux"
                                HINTS ${PC_WebPDemux_LIBDIR})
find_library(WebP_mux_LIBRARY NAMES "webpmux"
                              HINTS ${PC_WebPMux_LIBDIR})

find_path(WebP_INCLUDE_DIR NAMES "webp/decode.h"
                                 HINTS ${PC_WebP_INCLUDEDIR})
find_path(WebP_demux_INCLUDE_DIR NAMES "webp/demux.h"
                                 HINTS ${PC_WebPDemux_INCLUDEDIR})
find_path(WebP_mux_INCLUDE_DIR NAMES "webp/mux.h"
                               HINTS ${PC_WebPMux_INCLUDEDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(dpwebp DEFAULT_MSG WebP_INCLUDE_DIR WebP_LIBRARY
                                                       WebP_demux_INCLUDE_DIR WebP_demux_LIBRARY
                                                       WebP_mux_INCLUDE_DIR WebP_mux_LIBRARY)

mark_as_advanced(WebP_INCLUDE_DIR WebP_LIBRARY WebP_demux_INCLUDE_DIR WebP_demux_LIBRARY WebP_mux_INCLUDE_DIR WebP_mux_LIBRARY)
if(dpwebp_FOUND)
    set(WebP_FOUND ON)
    add_library(dpwebp::dpwebp INTERFACE IMPORTED)
    target_link_libraries(dpwebp::dpwebp INTERFACE ${WebP_LIBRARY} ${WebP_demux_LIBRARY} ${WebP_mux_LIBRARY})
    target_include_directories(dpwebp::dpwebp
                               INTERFACE ${WebP_INCLUDE_DIR} ${WebP_demux_INCLUDE_DIR} ${WebP_mux_INCLUDE_DIR})
endif()
