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

### Event mechanism

#set ( EV_FOUND OFF )

if ( EPOLL )
	check_include_file ( sys/epoll.h EV_EPOLL )
	
	if ( EV_EPOLL )
		set ( EV_FOUND ON )
	endif ( EV_EPOLL )
endif ( EPOLL )

if ( NOT EV_FOUND )
	if ( KQUEUE )
		include ( CheckCSourceCompiles )
		check_c_source_compiles(
			"#include <sys/event.h>\nint main() { return kqueue(); }"
			EV_KQUEUE
		)
		if ( EV_KQUEUE )
			set ( EV_FOUND ON )
		endif ( EV_KQUEUE )
	endif ( KQUEUE )
endif ( NOT EV_FOUND )

if ( NOT EV_FOUND )
	if ( PSELECT )
		check_function_exists ( pselect EV_PSELECT )
		if ( EV_PSELECT )
			set ( EV_FOUND ON )
		endif ( EV_PSELECT )
	endif ( PSELECT )
endif ( NOT EV_FOUND )

# fall back to select(2)
if ( NOT EV_FOUND )
	if ( SELECT )
		check_include_file ( "sys/select.h" EV_SELECT )
		if ( EV_SELECT )
			set ( EV_FOUND ON )
		endif ( EV_SELECT )
		
		# FD_COPY macro
		if ( EV_SELECT )
			if ( NOT WIN32 )
				check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" EV_SELECT_COPY )
			endif ( NOT WIN32 )
		endif ( EV_SELECT )
	endif ( SELECT )
endif ( NOT EV_FOUND )

if ( WIN32 )
	check_include_file ( winsock2.h HAVE_WSA )
	if ( HAVE_WSA )
		message( STATUS "Windows Socket API found" )
		if ( NOT EV_FOUND )
			if ( WSA )
				set ( EV_WSA ON )
				set ( EV_FOUND ON )
			else ( WSA )
				set ( EV_SELECT ON )
				set ( EV_FOUND ON )
			endif ( WSA )
		endif ( NOT EV_FOUND )
	endif ( HAVE_WSA )
endif ( WIN32 )

if ( NOT HAVE_WSA )
	message ( STATUS "Using POSIX Socket API" )
endif ( NOT HAVE_WSA )

if ( NOT EV_FOUND )
	status ( FATAL_ERROR "No event system found!" )
endif ( NOT EV_FOUND )

### Event mechanism sources.

set ( EV_SRC event.select.cpp )

if ( EV_WSA )
	set ( EV_SRC event.wsa.cpp )
endif ( EV_WSA )

if ( EV_KQUEUE )
	set ( EV_SRC event.kqueue.cpp )
endif ( EV_KQUEUE )

if ( EV_EPOLL )
	set ( EV_SRC event.epoll.cpp )
endif ( EV_EPOLL )

if ( EV_KEVENT )
	set ( EV_SRC event.kevent.cpp )
endif ( EV_KEVENT )

# config

configure_file ( config.h.cmake ${CMAKE_SOURCE_DIR}/src/server/config.h )
add_definitions ( -DHAVE_SRVCONFIG )

set ( WIN32RES "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	
	### generate resource file .o = ${WIN32RES}
	generate_win32_resource()
endif ( WIN32 )

link_libraries( libdrawpile ) # DP shared library

if ( NOT DEBUG )
	strip_exe ( ${INTERNALNAME} )
endif ( NOT DEBUG )

add_executable (
	${INTERNALNAME}
	MACOSX_BUNDLE
	dpsrv.cpp
	server.cpp
	sockets.cpp
	${EV_SRC}
	${WIN32RES}
)

set_target_properties (
	${INTERNALNAME}
	PROPERTIES
	VERSION 0.5.0
	PROJECT_LABEL drawpile-server
)

install ( TARGETS ${SERVERNAME} RUNTIME DESTINATION bin )
