project ( drawpile CXX C )

cmake_minimum_required( VERSION 2.8 )
set ( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/config )

### generic info
set ( WEBSITE "http://drawpile.sourceforge.net/" )
set ( BUGS "http://drawpile.sourceforge.net/tracker/" )
set ( DRAWPILE_VERSION "0.7.0" )

###
include ( "config/Names.cmake" )

### options ###
option ( CLIENT "Compile client" ON )
option ( SERVER "Compile UI-less server" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( GENERIC "Optimize for generic CPU arch" OFF )

option ( RELEASE "Enable final all-in-one compilation." OFF )

option ( BUILD_SHARED_LIBS "Build shared libraries." ON)

# Set build type
if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( )
	set ( CMAKE_BUILD_TYPE Release )
endif ( )

message ( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )

### Optimize ###
include ( "config/Optimize.cmake" )

# Include some nice macros
include ( "config/Macros.cmake" )

### binary and library output ###
set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib )

set ( QT_USE_QTNETWORK true )
SET ( QT_USE_QTXML true )
find_package ( Qt4 REQUIRED )

find_package ( ZLib REQUIRED )

### Output config.h ###
configure_file ( config/config.h.cmake ${CMAKE_BINARY_DIR}/config.h )
add_definitions ( -DHAVE_CONFIG_H )
# Tell the compiler where to find config.h
include_directories ( ${CMAKE_BINARY_DIR} )

# scan sub-directories
add_subdirectory( src )
add_subdirectory( doc )

if ( CMAKE_BUILD_TYPE STREQUAL Debug )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_DEBUG}")
elseif ( CMAKE_BUILD_TYPE STREQUAL Release )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif ( )

