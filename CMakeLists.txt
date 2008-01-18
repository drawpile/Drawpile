project ( drawpile CXX C )

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
option ( TRAYSERVER "Compile tray server" OFF )
option ( SERVERLIB "Compile server library" OFF )

option ( UNITTEST "Compile unit tests" OFF )

option ( EPOLL "Enable epoll(4) support (Linux 2.6)" OFF )
option ( SELECT "Enable select(2) support (any; fallback option)" ON )

option ( DEFLATE "Enable deflate compression (requires zlib)" ON )

option ( IPV6 "Enable IPv6 support" OFF )
option ( IPV4 "Enable IPv4 support" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( GENERIC "Optimize for generic CPU arch" OFF )
option ( SHARED "Build shared libraries." OFF ) # works in win32, other systems untested

# Our own SHA-1 implementation is broken, so using OpenSSL will effectively create
# incompatibility with servers/clients that use it.
option ( OPENSSL "Use OpenSSL if present" OFF )
option ( PERSISTENT "Enable persistent session support" OFF )
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

# Enable dependencies
set ( SERVERLIB false )
if ( SERVER OR TRAYSERVER )
	set ( SERVERLIB true )
endif ( )

### Optimize ###
include ( "config/Optimize.cmake" )

# Include some nice macros
include ( "config/Macros.cmake" )

### binary and library output ###
set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

if ( SHARED )
	set ( BUILD_SHARED_LIBS ON )
endif ( )

### config.h ###

### snprintf
include (CheckFunctionExists)
check_function_exists ( snprintf HAVE_SNPRINTF ) # C99 function

### Boost library
find_package ( Boost 1.33 REQUIRED )

set ( HAVE_BOOST false )
if ( Boost_FOUND )
	message ( STATUS "Found Boost C++ Libraries" )
	include_directories( ${Boost_INCLUDE_DIRS} )
	link_directories( ${Boost_LIBRARY_DIRS} )
	set ( HAVE_BOOST true )
else ( )
	message ( FATAL_ERROR "Boost C++ Libraries missing!" )
endif ( )

### add src/ as base include directory ###
include_directories ( AFTER src/ )

### testing ###
if ( UNITTEST )
	enable_testing ()
endif ( )

### endianess tests ###
include ( TestBigEndian ) # needs C in project()
test_big_endian ( IS_BIG_ENDIAN )

### Additional testing
set ( QT_USE_QTNETWORK true )
if ( CLIENT OR TRAYSERVER )
	find_package ( Qt4 REQUIRED )
endif ( )

if ( QT4_FOUND )
	#STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" qt_majv "${QTVERSION}")
	STRING(REGEX REPLACE "^[0-9]+\\.([0-9])+\\.[0-9]+" "\\1" qt_minv "${QTVERSION}")
	#STRING(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" qt_patv "${QTVERSION}")
	set ( HAVE_QT43 true )
	if ( ${qt_minv} LESS 3 )
		set ( HAVE_QT43 false )
	endif ( ${qt_minv} LESS 3 )
endif ( )

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
