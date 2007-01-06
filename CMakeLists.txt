project ( DrawPile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4.5 )

### config options ###

set ( CLIENTVERSION "0.2" )
set ( SERVERVERSION "0.1" )
set ( LIBVERSION "0.1" )

set ( LIBNAME "drawpile-${LIBVERSION}" )
set ( CLIENTNAME drawpile )
set ( SERVERNAME drawpile-srv )

### options ###

# do these even work?

option ( NOCLIENT "Don't compile client" OFF )
option ( NOSERVER "Don't compile server" OFF )

option ( NOEPOLL "Disable epoll(4) support" OFF )
option ( NOPSELECT "Disable pselect(2) support" OFF )
option ( NOKQUEUE "Disable kqueue(2) support" OFF )
option ( NOKEVENT "Disable kevent(2) support" OFF )

option ( UNITTEST "Compile unittests" OFF )

option ( DEBUG "Enable debugging and asserts" OFF )

option ( NOOPT "Disable any and all optimizations" OFF )

### Optimize ###

if ( NOT NOOPT )
	include ( Optimize.cmake )
endif ( NOT NOOPT)

include ( Macros.cmake )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

### config.h ###

# check portability stuff

include ( CheckVariableExists )
include ( CheckIncludeFiles )

# used by server & shared
check_include_files ( stdint.h HAVE_STDINT_H )

### Boost library

find_package ( Boost 1.33 REQUIRED )

if ( Boost_FOUND )
	message ( STATUS "libBoost found" )
#check_variable_exists ( Boost_FOUND HAVE_BOOST )
	include_directories( ${Boost_INCLUDE_DIRS} )
	link_directories( ${Boost_LIBRARY_DIRS} )
else ( Boost_FOUND )
	message ( STATUS "libBoost missing" )
endif ( Boost_FOUND )

### Event mechanism

include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckVariableExists )
include ( CheckIncludeFiles )

#set ( EV_SELECT OFF )
#set ( EV_PSELECT OFF )
#set ( EV_EPOLL OFF )
#set ( EV_KQUEUE OFF )
#set ( EV_KEVENT OFF )

# EPOLL
if ( NOT NOEPOLL )
	check_include_files ( sys/epoll.h HAVE_EPOLL_H )
	
	if ( HAVE_EPOLL_H )
		#add_definitions ( -DEV_EPOLL )
		# add in config.h
		set ( EV_EPOLL ON )
	endif ( HAVE_EPOLL_H )
endif ( NOT NOEPOLL )

# KQUEUE
if ( NOT NOKQUEUE )
	check_function_exists ( kqueue EV_KQUEUE )
	if ( EV_KQUEUE )
		#add_definitions ( -DEV_KQUEUE )
	endif ( EV_KQUEUE )
endif ( NOT NOKQUEUE )

# PSELECT
if ( NOT NOPSELECT )
	check_function_exists ( pselect EV_PSELECT )
	if ( EV_PSELECT )
		#add_definitions ( -DEV_PSELECT )
	endif ( EV_PSELECT )
endif ( NOT NOPSELECT )

# fall back to select(2)
if ( NOT NOSELECT )
	check_include_files ( "sys/select.h" HAVE_SELECT_H )
	if ( HAVE_SELECT_H )
		set ( EV_SELECT ON )
	else ( HAVE_SELECT_H )
		check_function_exists ( select EV_SELECT )
	endif ( HAVE_SELECT_H )
	
	# FD_COPY macro
	if ( EV_SELECT )
		if ( NOT WIN32 )
			check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" EV_SELECT_COPY )
			if ( EV_SELECT_COPY )
				#add_definitions( -DEV_SELECT_COPY )
				message( STATUS "FD_COPY found" )
			endif ( EV_SELECT_COPY )
		endif ( NOT WIN32 )
	endif ( EV_SELECT )
endif ( NOT NOSELECT )

if ( WIN32 )
	check_include_files ( winsock2.h HAVE_WSA )
	#check_symbol_exists ( WSAEWOULDBLOCK "winsock2.h" HAVE_WSA )
	if ( HAVE_WSA )
		message( STATUS "Windows Socket API found")
	endif ( HAVE_WSA )
	
	if ( NOT EV_SELECT )
		message ( STATUS "Assuming select(2) exists" )
		set ( EV_SELECT ON )
	endif ( NOT EV_SELECT )
endif ( WIN32 )

###

if ( NOT DEBUG )
	message ( STATUS "Debugging disabled" )
else ( NOT DEBUG )
	message ( STATUS "Debugging enabled" )
endif ( NOT DEBUG )


set (CMAKE_BUILD_TYPE Release )

if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( DEBUG )
	set ( CMAKE_BUILD_TYPE Release )
endif ( DEBUG )


### mode tests ###

#! needs C in project ()
include ( TestBigEndian )
test_big_endian ( IS_BIG_ENDIAN )

### subdirs ###

export_library_dependencies( cmake.depend )

configure_file ( config.h.cmake config.h )
add_definitions ( -DHAVE_CONFIG_H )

add_subdirectory( src/ )
