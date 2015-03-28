cmake_minimum_required( VERSION 2.8.9 )

project ( drawpile CXX )

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/config")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

### generic info
set ( WEBSITE "http://drawpile.net/" )
set ( DRAWPILE_VERSION "0.9.8" )

### protocol versions
# see doc/protocol.md for protocol version history
set ( DRAWPILE_PROTO_MAJOR_VERSION 14 )
set ( DRAWPILE_PROTO_MINOR_VERSION 5 )
set ( DRAWPILE_PROTO_DEFAULT_PORT 27750 )

###
include ( "config/Names.cmake" )

### options ###
option ( CLIENT "Compile client" ON )
option ( SERVER "Compile UI-less server" ON )
option ( TOOLS "Compile extra tools" OFF )
option ( INSTALL_DOC "Install documents" ON )
option ( INITSYS "Init system integration" "systemd" )

if (NOT CMAKE_BUILD_TYPE)
	message(STATUS "No build type selected, default to Release")
	message(STATUS "Use -DCMAKE_BUILD_TYPE=debug to enable debugging")
	set(CMAKE_BUILD_TYPE "Release")
endif()


message ( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )

# Include some nice macros
include ( "config/Macros.cmake" )

### binary and library output ###
set ( EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin" )
set ( LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib" )

find_package ( PkgConfig )

if ( PKGCONFIG_FOUND )
	if ( INITSYS STREQUAL  "systemd" )
		pkg_check_modules ( SYSTEMD "libsystemd" )
	endif ()
endif ( PKGCONFIG_FOUND )

### General compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")

### Compiler-specific C++ 11 activation ###
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_LESS 4.7)
        # GCC < 4.7
        set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
    else ()
        # GCC >= 4.7 (set instead of append, because it needs to be first)
        set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # Clang++
    set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ ${CMAKE_CXX_FLAGS}")
else ()
    # Other C++ compilers
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif ()

### Set deployment target (OSX specific)
if ( APPLE )
	set(CMAKE_CXX_FLAGS "-mmacosx-version-min=10.7 ${CMAKE_CXX_FLAGS}")
endif (APPLE)

### Output config.h ###
configure_file ( config/config.h.cmake "${CMAKE_BINARY_DIR}/config.h" )
add_definitions ( -DHAVE_CONFIG_H )
# Tell the compiler where to find config.h
include_directories ( "${CMAKE_BINARY_DIR}" )

# scan sub-directories
add_subdirectory( src )

if ( INSTALL_DOC )
	add_subdirectory( doc )
endif ( )

add_subdirectory( desktop )

if ( CMAKE_BUILD_TYPE STREQUAL Debug )
	message ( STATUS "CXX flags (debug): ${CMAKE_CXX_FLAGS_DEBUG}")
elseif ( CMAKE_BUILD_TYPE STREQUAL Release )
	message ( STATUS "CXX flags (release): ${CMAKE_CXX_FLAGS_RELEASE}")
endif ( )

