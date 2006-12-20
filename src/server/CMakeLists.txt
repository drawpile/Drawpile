# src/server/

### Options ###

option ( NOEPOLL "Disable epoll(4) support" OFF )
option ( NOPSELECT "Disable pselect(2) support" OFF )
option ( NOKQUEUE "Disable kqueue(2) support" OFF )
option ( NOKEVENT "Disable kevent(2) support" OFF )

### Find ###

set ( BOOST_LIB_DIAGNOSTIC true )

find_package ( Boost 1.33 QUIET )

if ( Boost_FOUND )
	message ( "++ libBoost found" )
	add_definitions( -DHAVE_BOOST )
	include_directories( ${Boost_INCLUDE_DIRS} )
	link_directories( ${Boost_LIBRARY_DIRS} )
else ( Boost_FOUND )
	message ( "++ libBoost not found" )
endif ( Boost_FOUND )

### Tests ###

include ( CheckSymbolExists )
include ( CheckFunctionExists )
#include ( CheckVariableExists )
include ( CheckIncludeFiles )

set ( EV_SELECT OFF CACHE BOOL "select(2) event mechanism" )
set ( EV_PSELECT OFF CACHE BOOL "pselect(2) event mechanism" )
set ( EV_EPOLL OFF CACHE BOOL "epoll(4) event mechanism" )
set ( EV_KQUEUE OFF CACHE BOOL "kqueue(2) event mechanism" )
set ( EV_KEVENT OFF CACHE BOOL "kevent(2) event mechanism" )
set ( NDEBUG ON CACHE BOOL "No debug; disables assert()" )

# EPOLL
if ( NOT NOEPOLL )
	check_include_files ( "sys/epoll.h" EV_EPOLL )
	if ( EV_EPOLL )
		#add_definitions ( -DEV_EPOLL )
		message("++ epoll(4) event mechanism found")
	endif ( EV_EPOLL )
endif ( NOT NOEPOLL )

# KQUEUE
if ( NOT NOKQUEUE )
	check_function_exists ( kqueue EV_KQUEUE )
	if ( EV_KQUEUE )
		#add_definitions ( -DEV_KQUEUE )
		message("++ kqueue(2) event mechanism found")
	endif ( EV_KQUEUE )
endif ( NOT NOKQUEUE )

# PSELECT
if ( NOT NOPSELECT )
	check_function_exists ( pselect EV_PSELECT )
	if ( EV_PSELECT )
		#add_definitions ( -DEV_PSELECT )
		message("++ pselect(2) event mechanism found")
	endif ( EV_PSELECT )
endif ( NOT NOPSELECT )

# fall back to select(2)
if ( NOT NOSELECT )
	check_function_exists ( select EV_SELECT )
	if ( EV_SELECT )
		#add_definitions ( -DEV_SELECT )
	endif ( EV_SELECT )
	
	# find sys/select.h
	check_include_files ( "sys/select.h" HAVE_SELECT_H )
	if ( NOT EV_SELECT )
		if ( HAVE_SELECT_H )
			message ( "++ Assuming select(2) exists" )
			set ( EV_SELECT ON )
		endif ( HAVE_SELECT_H )
	endif ( NOT EV_SELECT )
	
	if ( EV_SELECT )
		message("++ select(2) event mechanism found")
	endif ( EV_SELECT )
	
	# FD_COPY macro
	if ( NOT WIN32 )
		check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" EV_SELECT_COPY )
		if ( EV_SELECT_COPY )
			#add_definitions( -DEV_SELECT_COPY )
			message( "++ FD_COPY found" )
		endif ( EV_SELECT_COPY )
	endif ( NOT WIN32 )
endif ( NOT NOSELECT )

if ( WIN32 )
	check_include_files ( winsock2.h HAVE_WSA )
	#check_symbol_exists ( WSAEWOULDBLOCK "winsock2.h" HAVE_WSA )
	if ( HAVE_WSA )
		message("++ Windows Socket API found")
	endif ( HAVE_WSA )
	
	if ( NOT EV_SELECT )
		message ( "++ Assuming select(2) exists" )
		set ( EV_SELECT ON )
	endif ( NOT EV_SELECT )
endif ( WIN32 )

### Compiling ###

set (
	Server_Headers
	server.h
	user.h
	event.h
	sockets.h
	buffer.h
)

set (
	Server_Sources
	dpsrv.cpp
	server.cpp
	event.cpp
	sockets.cpp
)

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

include ( AddFileDependencies )
add_file_dependencies (
	dpsrv
	src/shared
)

link_libraries( dpshared )

if (MACOSX)
	set ( EXEFORMAT MACOSX_BUNDLE )
endif (MACOSX)

add_executable ( dpserver ${EXEFORMAT} ${Server_Sources} ${Server_Headers} )
