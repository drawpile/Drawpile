project ( drawpile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4 FATAL_ERROR )

### generic info

set ( WEBSITE "http://drawpile.sourceforge.net/" )
set ( BUGS "http://drawpile.sourceforge.net/tracker/" )

###

include ( config/SVN.cmake )

### options ###

option ( CLIENT "Compile client" ON )
option ( SERVER "Compile UI-less server" ON )
option ( TRAYSERVER "Compile tray server" ON )
option ( SERVERLIB "Compile server library; required by both server implementations" ON )

option ( UNITTEST "Compile unit tests" OFF )

option ( EPOLL "Enable epoll(4) support (Linux 2.6)" ON )
option ( KQUEUE "Enable kqueue(2) support (BSD/Mac[?])" ON )
option ( WSAPOLL "Enable WSAPoll event support (Vista/Server2003 and later)" ON )
option ( SELECT "Enable select(2) support (default)" ON )

option ( DEFLATE "Enable deflate compression (requires zlib)" ON )

option ( IPV6 "Enable IPv6 support [broken]" OFF )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( OPTIMIZE "Apply several generic optimization flags" ON )
option ( USEENV "Use CXXFLAGS as set in environemnt" ON )
option ( GENERIC "Optimize for generic CPU arch" OFF )
option ( SHARED "Build shared libraries." OFF ) # likely broken in some manner or another

option ( USE_URANDOM "Use /dev/urandom instead of rand()" ON )

#option ( UBINARY "Build universal binary (Mac-only)" ON ) # not implemented

option ( PROFILE "Enable profiling (-pg)" OFF )

option ( ASM "Use hand-written assembly" OFF )

# Our own SHA-1 implementation is broken, so using OpenSSL will effectively create
# incompatibility with servers/clients that use it.
option ( OPENSSL "Use OpenSSL if present" OFF )

option ( PERSISTENT "Enable persistent session support" OFF )

option ( RELEASE "Compile as much as possible in one go to generate more optimized binaries." OFF )

# detect Mac OS X

IF ( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
	set ( MACOSX ON )
	# lipo is used for generating universal binaries if gcc can't
	find_file ( LIPO_CMD lipo )
	set ( CMAKE_OSX_ARCHITECTURES "ppc;i386" )
endif ( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )

# Enable dependencies

if ( SERVER )
	set ( SERVERLIB ON )
endif ( SERVER )
if ( TRAYSERVER )
	set ( SERVERLIB ON )
endif ( TRAYSERVER )

# Set build type

if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( DEBUG )
	set ( CMAKE_BUILD_TYPE Release )
endif ( DEBUG )

### Optimize ###

if ( OPTIMIZE )
	include ( config/Optimize.cmake )
else ( OPTIMIZE )
	message ( STATUS "System CXX flags: $ENV{CXXFLAGS}" )
endif ( OPTIMIZE )

include ( config/Macros.cmake )

###

#set ( MTHREADS "-mthreads" )
#check_cxx_accepts_flag ( ${MTHREADS} ACCEPT_MTHREADS )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

if ( SHARED )
	set (BUILD_SHARED_LIBS ON )
endif ( SHARED )

### config.h ###

### snprintf

include (CheckFunctionExists)
check_function_exists ( snprintf HAVE_SNPRINTF ) # C99 function

### Boost library

find_package ( Boost 1.33 REQUIRED )

if ( Boost_FOUND )
	message ( STATUS "Boost C++ Libraries found" )
	include_directories( ${Boost_INCLUDE_DIRS} )
	link_directories( ${Boost_LIBRARY_DIRS} )
	set ( HAVE_BOOST ON )
else ( Boost_FOUND )
	message ( FATAL_ERROR "Boost C++ Libraries missing" )
endif ( Boost_FOUND )

### add src/ as base include directory ###

include_directories (
	AFTER
	src/
)

### testing ###

if ( UNITTEST )
	enable_testing ()
endif ( UNITTEST )

### endianess tests ###

include ( TestBigEndian ) # needs C in project()
test_big_endian ( IS_BIG_ENDIAN )

### Additional testing

set ( QT_USE_QTNETWORK ON )
if ( CLIENT )
	find_package ( Qt4 REQUIRED )
else ( CLIENT )
	if ( TRAYSERVER )
		find_package ( Qt4 REQUIRED )
	endif ( TRAYSERVER )
endif ( CLIENT )

if ( QT4_FOUND )
	#STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" qt_majv "${QTVERSION}")
	STRING(REGEX REPLACE "^[0-9]+\\.([0-9])+\\.[0-9]+" "\\1" qt_minv "${QTVERSION}")
	#STRING(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" qt_patv "${QTVERSION}")
	set ( HAVE_QT43 ON )
	if ( ${qt_minv} LESS 3 )
		set ( HAVE_QT43 OFF )
	endif ( ${qt_minv} LESS 3 )
endif ( QT4_FOUND )

#add_subdirectory ( config/ )

### Assembly
if ( ASM )
	message ( STATUS "ASM enabled!" )
	set ( USE_ASM ON ) # for config.h
endif ( ASM )

### Output config.h ###
configure_file ( config/config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h )
add_definitions ( -DHAVE_CONFIG_H )

# scan sub-directories
add_subdirectory( src/ )

### subdirs ###

if ( DEBUG )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else ( DEBUG )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif ( DEBUG)

message ( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )
