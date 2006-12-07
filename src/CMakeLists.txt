project ( DrawPile CXX C )

cmake_minimum_required( VERSION 2.4.5 )

### config options ###

option ( NOCLIENT "Don't compile client" OFF )
option ( NOSERVER "Don't compile server" OFF )
option ( UNITTEST "Compile unittests" OFF )


### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../lib )


### config.h ###

configure_file ( config.h.cmake config.h )

include ( CheckIncludeFiles )
check_include_files ( stdint.h HAVE_STDINT_H )


### tests ###

include ( TestForSTDNamespace )

include ( TestBigEndian )
test_big_endian ( BIG_ENDIAN )

include ( CheckVariableExists )
check_variable_exists ( BIG_ENDIAN BIG_ENDIAN )

### subdirs ###

add_subdirectory ( shared/ )

if ( NOSERVER )
	#
else ( NOSERVER )
	add_subdirectory ( server/ )
endif ( NOSERVER )

if ( NOCLIENT )
	#
else ( NOCLIENT )
	add_subdirectory ( client/ )
endif ( NOCLIENT )

if ( UNITTEST )
	add_subdirectory ( unittest/ )
endif ( UNITTEST )
