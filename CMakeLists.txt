cmake_minimum_required(VERSION 3.18)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10 CACHE STRING "macOS deployment target")

set(DRAWPILE_PROTO_DEFAULT_PORT 27750)
set(DP_MIN_QT_VERSION_GUI 5.12)
set(DP_MIN_QT_VERSION_SERVER 5.11)

include(CMakeDependentOption)
include(FeatureSummary)

# Client options
option(CLIENT "Compile client" ON)
add_feature_info("Drawpile client (CLIENT)" CLIENT "")
option(BUILD_LABEL "A custom label to add to the version")

if(WIN32)
	cmake_dependent_option(KIS_TABLET "Enable custom tablet support code" OFF "CLIENT" OFF)
	add_feature_info("Custom tablet support code (KIS_TABLET)" KIS_TABLET "")
endif()

# Server options
option(SERVER "Compile dedicated server" OFF)
add_feature_info("Drawpile server (SERVER)" SERVER "")

cmake_dependent_option(SERVERGUI "Enable server GUI" ON "SERVER" OFF)
add_feature_info("Server GUI (SERVERGUI)" SERVERGUI "")

if(UNIX AND NOT APPLE)
	cmake_dependent_option(INSTALL_DOC "Install documentation" ON "SERVER" OFF)
	add_feature_info("Install documentation (INSTALL_DOC)" INSTALL_DOC "")

	cmake_dependent_option(INITSYS "Init system integration for server (options: \"\", \"systemd\")" "systemd" "SERVER" "")
	add_feature_info("Server init system integration (INITSYS)" INITSYS "")
endif()

# Tools options
option(TOOLS "Compile extra tools" OFF)
add_feature_info("Extra tools (TOOLS)" TOOLS "")

option(TESTS "Build unit tests" OFF)
add_feature_info("Unit tests (TESTS)" TESTS "")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. Please specify a"
	" build directory when running CMake"
	" (`${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B <dir_name>`).\n"
	"You should also delete these files, which have already been generated:\n"
	"* ${CMAKE_BINARY_DIR}/CMakeCache.txt\n"
	"* ${CMAKE_BINARY_DIR}/CMakeFiles")
endif()

file(STRINGS Cargo.toml dp_version LIMIT_COUNT 1 REGEX "^version[ \t]*=[ \t]*\"[^\"]*\"$")
if(dp_version MATCHES "^version[ \t]*=[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)(-[A-Za-z0-9.-]*)?(\\+[A-Za-z0-9.-]*)?\"")
	set(PROJECT_VERSION ${CMAKE_MATCH_1})
	set(PROJECT_VERSION_LABEL ${CMAKE_MATCH_2})
else()
	message(FATAL_ERROR "Invalid version in Cargo.toml")
endif()

set(dp_proto_regex "^[ \t]*\\[[ \t]*([0-9]+),[ \t]*([0-9]+),[ \t]*([0-9]+)[ \t]*\\][ \t]*$")
file(STRINGS src/dpcore/src/protocol/protover.version dp_proto_version LIMIT_COUNT 1 REGEX ${dp_proto_regex})
if(dp_proto_version MATCHES ${dp_proto_regex})
	set(DRAWPILE_PROTO_SERVER_VERSION ${CMAKE_MATCH_1})
	set(DRAWPILE_PROTO_MAJOR_VERSION ${CMAKE_MATCH_2})
	set(DRAWPILE_PROTO_MINOR_VERSION ${CMAKE_MATCH_3})
else()
	message(FATAL_ERROR "Invalid protover.version")
endif()

project(Drawpile
	VERSION ${PROJECT_VERSION}
	HOMEPAGE_URL https://drawpile.net
)

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

find_package(Git)
if(Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tags
		OUTPUT_VARIABLE PROJECT_GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
else()
	set(PROJECT_GIT_REVISION ${PROJECT_VERSION}-unknown)
endif()
message(STATUS "Git revision: ${PROJECT_GIT_REVISION}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/config)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP "Generated Files")
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER "Generated Targets")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
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
	add_compile_options(/utf-8 /W4 /wd4456 /wd4457)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-Wall -Wextra -pedantic)
endif()

include(AutoSourceGroup)

if(TESTS)
	enable_testing()
	include(Tests)
endif()

# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html
find_package(QT NAMES Qt6 Qt5 REQUIRED)
message(STATUS "Using Qt version ${QT_VERSION_MAJOR}")
set(QT_PACKAGE_NAME Qt${QT_VERSION_MAJOR})

include(GNUInstallDirs)
include(Cargo)

if(CLIENT OR SERVER)
	find_package(${QT_PACKAGE_NAME} ${QT_MIN_VERSION_SERVER} REQUIRED COMPONENTS Core Network)

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

feature_summary(WHAT PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)

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
