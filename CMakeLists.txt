project ( drawpile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4 FATAL_ERROR )

### generic info

set ( WEBSITE "http://drawpile.sourceforge.net/" )
set ( BUGS "http://drawpile.sourceforge.net/tracker/" )

### options ###

# do these even work?

option ( CLIENT "Compile client" ON )
option ( SERVER "Compile UI-less server" ON )
option ( TRAYSERVER "Compile tray server" ON )
option ( SERVERLIB "Compile server library" ON )

option ( UNITTEST "Compile unit tests" OFF )

option ( EPOLL "Enable epoll(4) support" ON )
option ( PSELECT "Enable pselect(2) support" ON )
option ( KQUEUE "Enable kqueue(2) support" ON )
option ( KEVENT "Enable kevent(2) support" OFF )
option ( WSA "Enable WSA event support" OFF ) # still broken
option ( SELECT "Enable select(2) support" ON )

option ( DEFLATE "Enable deflate compression" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( OPTIMIZE "Apply several optimization flags" ON )
option ( GENERIC "Optimize for generic CPU arch" OFF )
option ( SHARED "Build shared library" OFF )

option ( UBINARY "Build universal binary (Mac-only)" ON )

option ( PROFILE "Enable profiling (-pg)" OFF )

option ( ASM "Use hand-written assembly" OFF )

option ( OPENSSL "Use OpenSSL if present" OFF )

option ( PERSISTENT "Enable persistent session support" OFF )

option ( RELEASE "" OFF )

option ( NOPI "Disable position-independent code and executables" OFF )

option ( UNSAFE_MATH "Enable unsafe math optimizations" OFF )
option ( OPTIMIZEMORE "Apply some questionable optimizations" ON )

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
