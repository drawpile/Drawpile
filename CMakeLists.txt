cmake_minimum_required(VERSION 3.18)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/config)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. Please specify a"
	" build directory when running CMake"
	" (`${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B <dir_name>`).\n"
	"You should also delete these files, which have already been generated:\n"
	"* ${CMAKE_BINARY_DIR}/CMakeCache.txt\n"
	"* ${CMAKE_BINARY_DIR}/CMakeFiles")
endif()

include(FeatureSummary)
include(DrawpileOptions)
include(DrawpileVersions)

project(Drawpile
	VERSION ${PROJECT_VERSION}
	HOMEPAGE_URL https://drawpile.net
)

# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html
find_package(QT NAMES Qt6 Qt5 REQUIRED)
message(STATUS "Using Qt version ${QT_VERSION_MAJOR}")
set(QT_PACKAGE_NAME Qt${QT_VERSION_MAJOR})

# There is an inherent conflict that the docs say `CMAKE_OSX_DEPLOYMENT_TARGET`
# needs to be set before calling `enable_language` or `project`, but it is
# impossible to run `find_package` before calling one of those, so instead we
# default it to the absolute minimum above, and then increase it to whatever Qt
# requires here
include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

# This check must happen after `project()` because `CMAKE_CONFIGURATION_TYPES`
# is not populated by defaults until then
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(FATAL_ERROR "Required build type missing."
		" Re-run CMake and specify one of these build types:\n"
		"* -DCMAKE_BUILD_TYPE=Debug\n"
		"* -DCMAKE_BUILD_TYPE=Release\n"
		"* -DCMAKE_BUILD_TYPE=RelWithDebInfo\n"
		"* -DCMAKE_BUILD_TYPE=MinSizeRel")
endif()

# CMake does not allow labels on the version in the project command, but having
# the split version number is useful for other places, so just pretend like it
# does semver and maybe it will someday
set(PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_LABEL}")

message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Protocol version: ${DRAWPILE_PROTO_SERVER_VERSION}.${DRAWPILE_PROTO_MAJOR_VERSION}.${DRAWPILE_PROTO_MINOR_VERSION}")
message(STATUS "Git revision: ${PROJECT_GIT_REVISION}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP "Generated Files")
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER "Generated Targets")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CheckIPOSupported)
check_ipo_supported(RESULT CMAKE_INTERPROCEDURAL_OPTIMIZATION LANGUAGES CXX)
add_feature_info("Interprocedural optimization" CMAKE_INTERPROCEDURAL_OPTIMIZATION "")
if (MSVC)
	add_compile_options(/utf-8 /W4 /wd4456 /wd4457 /WX)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	add_link_options(/WX:NO)

	# https://github.com/mozilla/sccache/pull/963
	if(CMAKE_C_COMPILER_LAUNCHER MATCHES "sccache")
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
			string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
		endif()
	endif()
	if(CMAKE_CXX_COMPILER_LAUNCHER MATCHES "sccache")
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
			string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
		endif()
	endif()
else()
	add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

include(AutoSourceGroup)

if(TESTS)
	enable_testing()
	include(Tests)
endif()

include(GNUInstallDirs)
include(Cargo)

if(CLIENT OR SERVER)
	include_directories(${CMAKE_SOURCE_DIR}/src)

	find_package(${QT_PACKAGE_NAME} ${QT_MIN_VERSION_SERVER} REQUIRED COMPONENTS Core Network)
	find_package(libsodium QUIET)

	if(libsodium_FOUND)
		add_compile_definitions(HAVE_LIBSODIUM)
	endif()
	add_feature_info("Ext-auth support" libsodium_FOUND "")

	#find_package(KF5DNSSD NO_MODULE) # disabled pending built-in server rewrite
	if(KF5DNSSD_FOUND)
		add_compile_definitions(HAVE_DNSSD)
		# KF5DNSSD 5.84.0 moves a bunch of headers to a different place.
		if("${KF5DNSSD_VERSION}" VERSION_LESS "5.84.0")
			add_compile_definitions(HAVE_DNSSD_BEFORE_5_84_0)
		endif()
	endif()

	message(STATUS "Generating config.h")
	configure_file(src/config.h.in config.h)
	include_directories(${CMAKE_CURRENT_BINARY_DIR})

	message(STATUS "Adding libshared")
	add_subdirectory(src/libshared)

	message(STATUS "Adding rustpile")
	add_cargo_library(rustpile PACKAGE rustpile)
endif()

if(CLIENT)
	message(STATUS "Adding libmypaint")
	add_subdirectory(src/dpmypaint/bundled/libmypaint)

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

	if(INSTALL_DOC)
		message(STATUS "Adding doc")
		add_subdirectory(doc)
	endif()
endif()

if(TOOLS)
	message(STATUS "Adding drawpile-cli")
	add_cargo_executable(drawpile-cli PACKAGE drawpile-cli)
endif()

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
