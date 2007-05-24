project ( DrawPile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4 FATAL_ERROR )

### generic info

set ( WEBSITE "http://drawpile.sourceforge.net/" )
set ( BUGS "http://drawpile.sourceforge.net/tracker/" )

### options ###

# do these even work?

option ( CLIENT "Compile client" ON )
option ( SERVER "Compile server" ON )

option ( EPOLL "Enable epoll(4) support" OFF )
option ( PSELECT "Enable pselect(2) support" OFF )
option ( KQUEUE "Enable kqueue(2) support" OFF )
option ( KEVENT "Enable kevent(2) support" OFF )
option ( WSA "Enable WSA event support" OFF ) # still broken
option ( SELECT "Enable select(2) support" ON )

option ( DEFLATE "Enable deflate compression" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( OPTIMIZE "Apply several optimization flags" ON )
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

###

set ( MTHREADS "-mthreads" )
check_cxx_accepts_flag ( ${MTHREADS} ACCEPT_MTHREADS )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

if ( SHARED )
	set ( LIBTYPE SHARED )
else ( SHARED )
	set ( LIBTYPE STATIC )
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

### endianess tests ###

include ( TestBigEndian ) # needs C in project()
test_big_endian ( IS_BIG_ENDIAN )

### Output config.h ###
configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h )
add_definitions ( -DHAVE_CONFIG_H )

# scan sub-directories
add_subdirectory( src/ )

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
