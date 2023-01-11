cmake_minimum_required(VERSION 3.15)

project(drawpile
	VERSION "2.2.0"
	LANGUAGES C CXX)

set(DRAWPILE_VERSION_SUFFIX "dancepiledev10")
set(DRAWPILE_VERSION "${PROJECT_VERSION}${DRAWPILE_VERSION_SUFFIX}")
set(DRAWPILE_HOMEPAGE_URL "https://drawpile.net/")

# protocol versions, see doc/protocol.md for protocol version history
set(DRAWPILE_PROTO_SERVER_VERSION 4)
set(DRAWPILE_PROTO_MAJOR_VERSION 22)
set(DRAWPILE_PROTO_MINOR_VERSION 2)
set(DRAWPILE_PROTO_DEFAULT_PORT 27750)

# scaffolding for Qt
find_package(ECM REQUIRED NOMODULE)
set(CMAKE_AUTOMOC ON)
set(CMAKE_MODULE_PATH
	"${CMAKE_MODULE_PATH}"
	"${CMAKE_SOURCE_DIR}/config"
	"${ECM_MODULE_PATH}"
	"${ECM_KDE_MODULE_DIR}")
include(KDEInstallDirs)

include("config/BuildOptions.cmake")
include("config/AddressSanitizer.cmake")
include("config/ClangTidy.cmake")
include("config/Macros.cmake")
include("config/Paths.cmake")
include("config/Dependencies.cmake")

if(TESTS)
	enable_testing()
	include("config/Tests.cmake")
endif()

function(set_drawpile_target_properties target)
	target_compile_definitions("${target}" PRIVATE
		"DP_PERF_REALM=${target}"
		"HAVE_CONFIG_H")
	if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
		target_compile_options("${target}" PRIVATE "-pedantic" "-Wall" "-Wextra")
	elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
		target_compile_options("${target}" PRIVATE "/W4" "/wd4267" "/wd4305" "/wd4456" "/wd4457" "/wd4458" "/wd4099" "/utf-8")
	endif()
	if(APPLE)
		target_compile_options("${target}" PRIVATE "-mmacosx-version-min=10.10")
	endif()
	if(WIN32)
		target_compile_definitions("${target}" PRIVATE _CRT_SECURE_NO_WARNINGS) # disable _s warnings
	endif()
	if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		target_compile_definitions("${target}" PRIVATE "QT_NO_DEBUG_OUTPUT")
	endif()
	target_include_directories("${target}" PUBLIC "${CMAKE_BINARY_DIR}")
	set_target_properties("${target}" PROPERTIES
		C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
		CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
	add_address_sanitizer("${target}")
	add_clang_tidy("${target}")
endfunction()

configure_file(config/config.h.cmake "${CMAKE_BINARY_DIR}/config.h")

file(WRITE "${CMAKE_BINARY_DIR}/DRAWPILE_VERSION.txt" ${DRAWPILE_VERSION}) # needed for packaging

add_subdirectory(src)
if(INSTALL_DOC)
	add_subdirectory(doc)
endif()
add_subdirectory(desktop)
