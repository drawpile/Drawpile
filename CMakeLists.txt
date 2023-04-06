cmake_minimum_required(VERSION 3.18)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. Please specify a"
	" build directory when running CMake"
	" (`${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B <dir_name>`).\n"
	"You should also delete these files, which have already been generated:\n"
	"* ${CMAKE_BINARY_DIR}/CMakeCache.txt\n"
	"* ${CMAKE_BINARY_DIR}/CMakeFiles")
endif()

# This must come before `project` because the version it pulls from an external
# file is used as the CMake project version
include(DrawpileVersions)

project(Drawpile
	VERSION ${PROJECT_VERSION}
	HOMEPAGE_URL https://drawpile.net
)

# DrawpileOptions uses this so it must be included before then
include(FeatureSummary)

# This must come after `project` because it relies on variables like `ANDROID`
# that are only set once that is called
include(DrawpileOptions)

# This cannot go in `DrawpileVersions.cmake` because it relies on stuff from
# `DrawpileOptions`
if(CLIENT OR SERVERGUI)
	set(DP_MIN_QT_VERSION 5.12)
else()
	# The minimum for headless servers is different than GUI because the current
	# Debian LTS provides only this older version of Qt, but it is too painful
	# to not have at least 5.12 when doing GUI stuff
	set(DP_MIN_QT_VERSION 5.11)
endif()

# This check must happen after `project()` because `CMAKE_CONFIGURATION_TYPES`
# is not populated until then
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(FATAL_ERROR "Required build type missing."
		" Re-run CMake and specify one of these build types:\n"
		"* -DCMAKE_BUILD_TYPE=Debug\n"
		"* -DCMAKE_BUILD_TYPE=Release\n"
		"* -DCMAKE_BUILD_TYPE=RelWithDebInfo\n"
		"* -DCMAKE_BUILD_TYPE=MinSizeRel")
endif()

if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	message(WARNING "Using ${PROJECT_NAME} as a subproject is untested.")
endif()

# CMake does not allow labels on the version in the project command, but having
# the split version number is useful for other places, so just pretend like it
# does semver and maybe it will someday
set(PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_LABEL}")

# All of this information should be known by this point
message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Protocol version: ${DRAWPILE_PROTO_SERVER_VERSION}.${DRAWPILE_PROTO_MAJOR_VERSION}.${DRAWPILE_PROTO_MINOR_VERSION}")
message(STATUS "Git revision: ${PROJECT_GIT_REVISION}")

# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html
find_package(QT ${DP_MIN_QT_VERSION} NAMES Qt6 Qt5 REQUIRED)
message(STATUS "Using Qt version ${QT_VERSION_MAJOR}")
set(QT_PACKAGE_NAME Qt${QT_VERSION_MAJOR})
# Qt5AndroidSupport.cmake clobbers this variable, so make a copy
set(DP_QT_DIR ${QT_DIR})

# There is an inherent conflict where the docs say `CMAKE_OSX_DEPLOYMENT_TARGET`
# needs to be set before calling `enable_language` or `project`, but it is
# impossible to run `find_package` before calling one of those and we need to
# do that to learn which Qt version we are using since that defines what the
# minimum deployment target is. So, we default it to the absolute minimum at the
# start of the file, and then increase it to whatever Qt requires here
include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

include(AutoSourceGroup)
include(DrawpileInstallDirs)

# This *must* be included before `find_package(Qt5 COMPONENTS Core)` because
# `find_package` loads `Qt5AndroidSupport.cmake` which immediately creates the
# `apk` target and writes out `android_deployment_settings.json` using global
# variables.
if(ANDROID AND QT_VERSION VERSION_LESS 6)
	include(Qt5AndroidDeploymentTarget)
endif()

if(CLIENT OR SERVER)
	# Since Drawdance also runs `find_package(Qt)` we must run it first or else
	# the `apk` target will pick up the sub-project `PROJECT_NAME` instead of
	# our own `PROJECT_NAME`.
	find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Core Network)

	if(EXISTS ${PROJECT_SOURCE_DIR}/extern/drawdance/CMakeLists.txt)
		add_subdirectory(extern/drawdance EXCLUDE_FROM_ALL)
	else()
		find_package(Drawdance REQUIRED COMPONENTS dpcommon dpengine dpmsg)
	endif()
endif()

# This must be included *after* drawdance is added to make sure that we do not
# pollute that sub-project with our compiler options. Once CMake 3.25 is the
# minimum required version and Qt5 for Android is not used any more, it is
# possible that the SYSTEM flag of `add_subdirectory` may do this and then this
# can be moved down the list to where the rest of the dependencies are.
include(DrawpileCompilerOptions)

if(TESTS)
	enable_testing()
	include(Tests)
endif()

# All intra-project includes should be fully qualified relative to the root
# source directory to not go insane figuring out what is included from where
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

if(CLIENT OR SERVER)
	# Everything needs these dependencies so just do it in one place
	find_package(${QT_PACKAGE_NAME} QUIET COMPONENTS LinguistTools)
	find_package(libsodium QUIET)

	if(ANDROID AND QT_VERSION VERSION_LESS 6)
		find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS AndroidExtras)
	endif()

	if(TARGET libsodium::libsodium)
		add_compile_definitions(HAVE_LIBSODIUM)
	endif()

	# LinguistTools does not generate a target so its existence must be checked
	# using the old-style FOUND flag
	add_feature_info("Client translations" ${QT_PACKAGE_NAME}LinguistTools_FOUND "")
	add_feature_info("Ext-auth support" "TARGET libsodium::libsodium" "")

	#find_package(KF5DNSSD NO_MODULE) # disabled pending built-in server rewrite
	if(TARGET KF5::DNSSD)
		add_compile_definitions(HAVE_DNSSD)
		# KF5DNSSD 5.84.0 moves a bunch of headers to a different place.
		if("${KF5DNSSD_VERSION}" VERSION_LESS "5.84.0")
			add_compile_definitions(HAVE_DNSSD_BEFORE_5_84_0)
		endif()
	endif()
	add_feature_info("Zeroconf support" "TARGET KF5::DNSSD" "")

	message(STATUS "Generating config.h")
	include(DrawpileFileExtensions)
	get_qt_extensions(RECORDING DRAWPILE_FILE_GROUP_RECORDING)
	get_qt_extensions(IMAGE DRAWPILE_FILE_GROUP_IMAGE)
	configure_file(src/config.h.in config/config.h)
	include_directories(${CMAKE_CURRENT_BINARY_DIR}/config)

	message(STATUS "Adding libshared")
	add_subdirectory(src/libshared)
endif()

if(CLIENT)
	message(STATUS "Adding libclient")
	add_subdirectory(src/libclient)

	message(STATUS "Adding desktop")
	add_subdirectory(src/desktop)
endif()

if(SERVER)
	message(STATUS "Adding libserver")
	add_subdirectory(src/libserver)

	message(STATUS "Adding thinsrv")
	add_subdirectory(src/thinsrv)
endif()

if(TOOLS)
	message(STATUS "Someone deleted the tools")
endif()

# This must run once all target creation is finished since it walks the list of
# targets and their sources. This could be called any time and use
# `cmake_language(DEFER)` once CMake 3.19 is the minimum supported version.
disable_automoc_warnings()

# Since Android is cross-compiled it is already always creating a distributable
# build
if(DIST_BUILD AND NOT ANDROID)
	include(DrawpileDistBuild)
endif()

include(DrawpilePackaging)

feature_summary(INCLUDE_QUIET_PACKAGES
	WHAT PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)

if(CMAKE_CONFIGURATION_TYPES)
	message(NOTICE
		"++ For a debug build, run: `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}"
		" --config Debug`\n"
		"++ For a release build, run: `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}"
		" --config Release`")
else()
	message(NOTICE
		"++ To build, run: `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}`")
endif()
