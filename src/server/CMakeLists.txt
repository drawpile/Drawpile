# src/server/

set ( FULLNAME "DrawPile server" )
set ( INTERNALNAME "drawpile-srv" )
set ( COPYRIGHT "Copyright (C) 2006,2007 M.K.A." )
set ( DESCRIPTION ${FULLNAME} )
set ( FILE_COMMENT "http://drawpile.sourceforge.net/" )

set ( VERSION_MAJOR 0 )
set ( VERSION_MINOR 5 )
set ( VERSION_BUG 0 )

if ( WIN32 )
	set ( ORIGFILENAME "${INTERNALNAME}.exe" )
else ( WIN32 )
	set ( ORIGFILENAME "${INTERNALNAME}" )
endif ( WIN32 )

# include some stuff

include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )
include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckVariableExists )

### For deflate extension

if ( DEFLATE )
	find_package ( ZLIB )
	if ( ZLIB_FOUND )
		set ( HAVE_ZLIB ON )
		link_libraries ( ${ZLIB_LIBRARIES} )
	else ( ZLIB_FOUND )
		check_include_file ( zlib.h HAVE_ZLIB )
		if ( HAVE_ZLIB )
			link_libraries ( "z" )
		else ( HAVE_ZLIB )
			message ( STATUS "ZLIB disabled!" )
			set ( DEFLATE OFF )
		endif ( HAVE_ZLIB )
	endif ( ZLIB_FOUND )
endif ( DEFLATE )

### fork()

check_function_exists ( fork HAVE_FORK )
check_function_exists ( fork1 HAVE_FORK1 )

if ( HAVE_FORK1 )
	set ( HAVE_FORK OFF )
	link_libraries ( pthread )
endif ( HAVE_FORK1 )

if ( HAVE_FORK )
	link_libraries ( pthread )
endif ( HAVE_FORK )

# config

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_SRVCONFIG )

set ( WIN32RES "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	
	### generate resource file .o = ${WIN32RES}
	generate_win32_resource()
endif ( WIN32 )

link_libraries( libdrawpile ) # DP shared library
link_libraries( events ) # this name might conflict

add_subdirectory ( ev/ ) # events lib

if ( NOT DEBUG )
	strip_exe ( ${INTERNALNAME} )
endif ( NOT DEBUG )

add_executable (
	${INTERNALNAME}
	MACOSX_BUNDLE
	dpsrv.cpp
	server.cpp
	sockets.cpp
	${WIN32RES}
)

add_dependencies ( drawpile-srv eventslib libdrawpile )

set_target_properties (
	${INTERNALNAME}
	PROPERTIES
	VERSION 0.5.0
	PROJECT_LABEL drawpile-server
)

install ( TARGETS ${SERVERNAME} RUNTIME DESTINATION bin )
