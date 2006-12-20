project ( DrawPile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4.5 )

### Optimize ###

include ( Optimize.cmake )

### config options ###

# do these even work?

option ( NOCLIENT "Don't compile client" OFF )
option ( NOSERVER "Don't compile server" OFF )

option ( UNITTEST "Compile unittests" OFF )

option ( DEBUG "Enable debugging and asserts" OFF )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

### config.h ###

# check portability stuff

include ( CheckIncludeFiles )

# used by server & shared
check_include_files ( stdint.h HAVE_STDINT_H )

if ( NOT DEBUG )
	message ( "++ Debugging disabled" )
	set ( NDEBUG ON )
	#add_definitions( -DNDEBUG )
else ( NOT DEBUG )
	message ( "++ Debugging enabled" )
	set ( NDEBUG OFF )
endif ( NOT DEBUG )

### mode tests ###

#! needs C in project ()
include ( TestBigEndian )
test_big_endian ( IS_BIG_ENDIAN )

### subdirs ###

export_library_dependencies( cmake.depend )

configure_file ( config.h.cmake config.h )
add_definitions ( -DHAVE_CONFIG_H )

add_subdirectory( src/ )
