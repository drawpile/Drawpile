# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.18)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

project(Drawdance
    VERSION 0.3.0
    DESCRIPTION "A Drawpile Client"
    LANGUAGES C CXX)

set(CMAKE_UNITY_BUILD off)

if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "${PROJECT_NAME} building as sub-project")
    set(SUBPROJECT on)
    set(SUBPROJECT_INCLUDE SYSTEM)
else()
    include(GNUInstallDirs)
endif()

include(FeatureSummary)
include(CMakePackageConfigHelpers)

if(NOT CMAKE_CROSSCOMPILING)
    option(USE_GENERATORS "Do code generation" ON)
    add_feature_info("Code generation (USE_GENERATORS)" USE_GENERATORS "")
endif()

if(NOT MSVC AND NOT EMSCRIPTEN AND NOT SUBPROJECT)
    # Feature info will be emitted later once it is clear whether or not these
    # features could actually be enabled
    option(CLANG_TIDY "Automatically enable Clang-Tidy" ON)
    option(ADDRESS_SANITIZER "Automatically enable AddressSanitizer" OFF)

    option(USE_STRICT_ALIASING "Enable strict aliasing optimizations" OFF)
    add_feature_info("Strict aliasing (USE_STRICT_ALIASING)" USE_STRICT_ALIASING "")

    option(ENABLE_ARCH_NATIVE "Optimize for this computer's CPU" OFF)
    add_feature_info("Non-portable optimizations (ENABLE_ARCH_NATIVE)" ENABLE_ARCH_NATIVE "")
endif()

if(NOT SUBPROJECT)
    option(ENABLE_WERROR "Fail the build when there's warnings" OFF)
    add_feature_info("Warning as errors (ENABLE_WERROR)" ENABLE_WERROR "")

    option(BUILD_APPS "Build applications (as opposed to only libraries)" ON)
    add_feature_info("Applications (BUILD_APPS)" BUILD_APPS "")

    option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
    add_feature_info("Shared libraries (BUILD_SHARED_LIBS)" BUILD_SHARED_LIBS "")
endif()

if(SUBPROJECT)
    set(IMAGE_IMPL QT)
    set(FILE_IO_IMPL QT)
    set(ZIP_IMPL LIBZIP)
elseif(EMSCRIPTEN)
    set(IMAGE_IMPL LIBS)
    set(FILE_IO_IMPL STDIO)
else()
    set(IMAGE_IMPL LIBS CACHE STRING "PNG and JPEG implementation (LIBS, QT)")
    string(TOUPPER IMAGE_IMPL ${IMAGE_IMPL})

    set(FILE_IO_IMPL STDIO CACHE STRING "Default file I/O (STDIO, QT)")
    string(TOUPPER FILE_IO_IMPL ${FILE_IO_IMPL})

    option(BUILD_TESTS "Build TAP tests" ON)
    add_feature_info("TAP tests (BUILD_TESTS)" BUILD_TESTS "")

    option(BUILD_TOOLS "Build command-line tools, even if BUILD_APPS is OFF" OFF)
    add_feature_info("Tools (BUILD_TOOLS)" BUILD_TOOLS "")

    set(ZIP_IMPL LIBZIP CACHE STRING "ZIP folder implementation (LIBZIP, KARCHIVE)")
    string(TOUPPER ZIP_IMPL ${ZIP_IMPL})

    set(QT_VERSION 5 CACHE STRING "Qt version to use, if pulled in (5, 6)")
    add_feature_info("Qt version (QT_VERSION)" ON ${QT_VERSION})
    set(QT_VERSION_MAJOR ${QT_VERSION})
endif()

add_feature_info("Image library implementation (IMAGE_IMPL)" ON ${IMAGE_IMPL})
add_feature_info("File I/O implementation (FILE_IO_IMPL)" ON ${FILE_IO_IMPL})
add_feature_info("ZIP implementation (ZIP_IMPL)" ON ${ZIP_IMPL})

string(LENGTH "${PROJECT_SOURCE_DIR}/" project_dir_length)
add_compile_definitions(
    "DP_PROJECT_DIR_LENGTH=${project_dir_length}"
    _XOPEN_SOURCE=600
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

include(compile_flags)
include(helper)
include(dependencies)

configure_file(cmake/dpconfig.h.in "${PROJECT_BINARY_DIR}/config/dpconfig.h")

if(BUILD_TESTS)
    include(tests)
    add_subdirectory(test)
endif()

add_subdirectory(3rdparty)
add_subdirectory(generators)
add_subdirectory(libcommon)
add_subdirectory(libmsg)
add_subdirectory(libengine)

if(BUILD_APPS)
    add_subdirectory(libclient)
    add_subdirectory(appdrawdance)
endif()

if(BUILD_TOOLS)
    add_subdirectory(appconv)
endif()

if(NOT SUBPROJECT)
    write_basic_package_version_file(
        DrawdanceVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    get_property(CONFIG_DEPS GLOBAL PROPERTY dp_dependencies)
    list(JOIN CONFIG_DEPS "\n" CONFIG_DEPS)
    get_property(CONFIG_COMPONENTS GLOBAL PROPERTY dp_components)
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    feature_summary(INCLUDE_QUIET_PACKAGES
        WHAT PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)
endif()

if(BUILD_TESTS)
    report_dptest_targets()
endif()
