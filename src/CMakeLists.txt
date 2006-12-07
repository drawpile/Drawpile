project (DrawPile)

# config options

option ( NOCLIENT "Do not compile&link client" )
option ( NOSERVER "Do not compile&link server" )

# binary and library output

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../lib )

# config.h

configure_file ( config.h.cmake config.h )

include ( CheckIncludeFiles )
check_include_files ( stdint.h HAVE_STDINT_H )

# subdirs

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
