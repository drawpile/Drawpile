project (DrawPile)

configure_file ( config.h.cmake config.h )

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../lib )

include ( CheckIncludeFiles )
check_include_files ( stdint.h HAVE_STDINT_H )

subdirs ( shared/ server/ client/ )
