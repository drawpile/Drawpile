project ( drawpile CXX )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4 FATAL_ERROR )
set ( CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true )

### generic info
set ( WEBSITE "http://drawpile.sourceforge.net/" )
set ( BUGS "http://drawpile.sourceforge.net/tracker/" )

###
include ( "config/Names.cmake" )
include ( "config/SVN.cmake" )

### options ###
option ( CLIENT "Compile client" ON )
option ( SERVER "Compile UI-less server" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( GENERIC "Optimize for generic CPU arch" OFF )
option ( SHARED "Build shared libraries." OFF ) # works in win32, other systems untested

option ( RELEASE "Enable final all-in-one compilation." OFF )

# Set build type
if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( )
	set ( CMAKE_BUILD_TYPE Release )
endif ( )

message ( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )

# detect Mac OS X (this is WRONG!)
set ( MACOS false)
if ( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
	set ( MACOS true )
endif ( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )

### Optimize ###
include ( "config/Optimize.cmake" )

# Include some nice macros
include ( "config/Macros.cmake" )

### binary and library output ###
set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

set ( BUILD_SHARED_LIBS ON )

### config.h ###

### add src/ as base include directory ###
include_directories ( AFTER src/ )

set ( QT_USE_QTNETWORK true )

find_package ( Qt4 REQUIRED )

#add_subdirectory ( config/ )

### Output config.h ###
configure_file ( config/config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h )
add_definitions ( -DHAVE_CONFIG_H )

# scan sub-directories
add_subdirectory( src/ )

### subdirs ###

if ( DEBUG )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else ( )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif ( )
