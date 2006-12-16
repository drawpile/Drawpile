project ( DrawPile CXX C )

cmake_minimum_required( VERSION 2.4.5 )

### config options ###

option ( NOCLIENT "Don't compile client" OFF )
option ( NOSERVER "Don't compile server" OFF )

option ( UNITTEST "Compile unittests" OFF )

option ( NOEPOLL "Disable epoll(4) support" OFF )
option ( NOPSELECT "Disable pselect(2) support" OFF )
option ( NOKQUEUE "Disable kqueue(2) support" OFF )
option ( NOKEVENT "Disable kevent(2) support" OFF )

option ( DEBUG "Enable debugging and asserts" OFF )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

### config.h ###

# check portability stuff

include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckIncludeFiles )
#include ( CheckVariableExists )

check_include_files ( stdint.h HAVE_STDINT_H )

#set ( EV_SELECT OFF CACHE BOOL "select(2) event mechanism" )
#set ( EV_PSELECT OFF CACHE BOOL "pselect(2) event mechanism" )
#set ( EV_EPOLL OFF CACHE BOOL "epoll(4) event mechanism" )
#set ( EV_KQUEUE OFF CACHE BOOL "kqueue(2) event mechanism" )
#set ( EV_KEVENT OFF CACHE BOOL "kevent(2) event mechanism" )
#set ( NDEBUG ON CACHE BOOL "No debug; disables assert()" )

# EPOLL
if ( NOT NOEPOLL )
	check_include_files ( sys/epoll.h EV_EPOLL )
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
		message ( "!! select(2) not found, even though it should've" )
	endif ( NOT EV_SELECT )
endif ( WIN32 )

if ( NOT DEBUG )
	message ( "++ Debugging disabled" )
	add_definitions( -DNDEBUG )
else ( NOT DEBUG )
	message ( "++ Debugging enabled" )
endif ( NOT DEBUG )

### tests ###

include ( TestForSTDNamespace )

#! needs C in project ()
include ( TestBigEndian )
test_big_endian ( IS_BIG_ENDIAN )
#if ( IS_BIG_ENDIAN )
	#check_variable_exists ( IS_BIG_ENDIAN IS_BIG_ENDIAN )
	#add_definitions( -DIS_BIG_ENDIAN )
#endif ( IS_BIG_ENDIAN )

### subdirs ###

add_subdirectory( src/ )

export_library_dependencies( cmake.depend )

configure_file ( config.h.cmake config.h )
add_definitions ( -DHAVE_CONFIG_H )
