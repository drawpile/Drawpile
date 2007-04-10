project ( DrawPile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4 FATAL_ERROR )

### config options ###

set ( CLIENTVERSION "0.4" )
set ( SERVERVERSION "0.3" )
set ( LIBVERSION "0.3" )

set ( LIBNAME "drawpile-${LIBVERSION}" )
set ( CLIENTNAME drawpile )
set ( SERVERNAME drawpile-srv )

### options ###

# do these even work?

option ( CLIENT "Don't compile client" ON )
option ( SERVER "Don't compile server" ON )
option ( UNITTEST "Compile unittests" OFF )

option ( EPOLL "Disable epoll(4) support" ON )
option ( PSELECT "Disable pselect(2) support" ON )
option ( KQUEUE "Disable kqueue(2) support" ON )
option ( KEVENT "Disable kevent(2) support" ON )
option ( WSA "Enable WSA event support" OFF ) # still broken

option ( DEFLATE "Disable deflate compression" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( OPTIMIZE "Disable any and all optimizations" ON )
option ( SHARED "Build shared library" OFF )

option ( PROFILE "Enable profiling (-pg)" OFF )

option ( UNSAFE_MATH "Enable unsafe math optimizations" OFF )

# Set build type

set (CMAKE_BUILD_TYPE Release )

if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( DEBUG )
	set ( CMAKE_BUILD_TYPE Release )
endif ( DEBUG )

### Optimize ###

if ( OPTIMIZE )
	include ( Optimize.cmake )
endif ( OPTIMIZE )

include ( Macros.cmake )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

if ( SHARED )
	set ( LIBTYPE SHARED )
else ( SHARED )
	set ( LIBTYPE STATIC )
endif ( SHARED )

### config.h ###

# check portability stuff

include ( CheckVariableExists )
#include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )

# used by server & shared
check_include_file_cxx ( stdint.h HAVE_STDINT_H )

### mode tests ###

#! needs C in project ()
include ( TestBigEndian )
test_big_endian ( IS_BIG_ENDIAN )

# scan sub-directories

add_subdirectory( src/ )

### Output config.h ###

#export_library_dependencies( cmake.depend )

configure_file ( config.h.cmake ${CMAKE_SOURCE_DIR}/src/config.h )
add_definitions ( -DHAVE_CONFIG_H )

### subdirs ###

if ( DEBUG )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else ( DEBUG )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif ( DEBUG)

### Build type ###

if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( DEBUG )
	set ( CMAKE_BUILD_TYPE Release )
endif ( DEBUG )

message ( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )
