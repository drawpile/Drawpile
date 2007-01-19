project ( DrawPile CXX C )

# CMake version requirement. CMake might whine on older version, but will
# work unless compatibility is broken.

cmake_minimum_required( VERSION 2.4 FATAL_ERROR )

### config options ###

set ( CLIENTVERSION "0.4" )
set ( SERVERVERSION "0.3" )
set ( LIBVERSION "0.2" )

set ( LIBNAME "drawpile-${LIBVERSION}" )
set ( CLIENTNAME drawpile )
set ( SERVERNAME drawpile-srv )

### options ###

# do these even work?

option ( CLIENT "Don't compile client" ON )
option ( SERVER "Don't compile server" ON )
option ( UNITTEST "Compile unittests" OFF )

option ( EPOLL "Disable epoll(4) support" ON )
option ( PSELECT "Disable pselect(2) support" ON )
option ( KQUEUE "Disable kqueue(2) support" ON )
option ( KEVENT "Disable kevent(2) support" ON )

option ( XMLCONFIG "Enable XML config file support" ON )

option ( BOEHMGC "Enable Boehm Garbage Collector" OFF )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( OPTIMIZE "Disable any and all optimizations" ON )
option ( SHARED "Build shared library" OFF )

### Build type ###

if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( DEBUG )
	set ( CMAKE_BUILD_TYPE Release )
endif ( DEBUG )

message ( STATUS "build type: ${CMAKE_BUILD_TYPE}" )

### Optimize ###

if ( OPTIMIZE )
	include ( Optimize.cmake )
endif ( OPTIMIZE )

include ( Macros.cmake )

### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

if ( SHARED )
	set ( LIBTYPE SHARED )
else ( SHARED )
	set ( LIBTYPE STATIC )
endif ( SHARED )

### config.h ###

# check portability stuff

include ( CheckVariableExists )
include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )

# used by server & shared
check_include_file ( stdint.h HAVE_STDINT_H )

### Boost library

find_package ( Boost 1.33 REQUIRED )

if ( Boost_FOUND )
	message ( STATUS "libBoost found" )
	check_variable_exists ( Boost_FOUND HAVE_BOOST )
	include_directories( ${Boost_INCLUDE_DIRS} )
	link_directories( ${Boost_LIBRARY_DIRS} )
else ( Boost_FOUND )
	message ( STATUS "libBoost missing" )
endif ( Boost_FOUND )

### TinyXML for Server ###

check_include_file_cxx ( tinyxml.h HAVE_TINYXML )
if ( HAVE_TINYXML )
	message ( STATUS "TinyXML found" )
else ( HAVE_TINYXML )
	message ( STATUS "TinyXML missing" )
endif ( HAVE_TINYXML )

### Event mechanism

include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckVariableExists )
include ( CheckIncludeFiles )

if ( EPOLL )
	check_include_file ( sys/epoll.h HAVE_EPOLL_H )
	
	if ( HAVE_EPOLL_H )
		#add_definitions ( -DEV_EPOLL )
		# add in config.h
		set ( EV_EPOLL ON )
	endif ( HAVE_EPOLL_H )
endif ( EPOLL )

if ( KQUEUE )
	check_function_exists ( kqueue EV_KQUEUE )
	if ( EV_KQUEUE )
		#add_definitions ( -DEV_KQUEUE )
	endif ( EV_KQUEUE )
endif ( KQUEUE )

if ( PSELECT )
	check_function_exists ( pselect EV_PSELECT )
	if ( EV_PSELECT )
		#add_definitions ( -DEV_PSELECT )
	endif ( EV_PSELECT )
endif ( PSELECT )

# fall back to select(2)
if ( SELECT )
	check_include_file ( "sys/select.h" HAVE_SELECT_H )
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
endif ( SELECT )

if ( WIN32 )
	check_include_file ( winsock2.h HAVE_WSA )
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

### Output config.h ###

export_library_dependencies( cmake.depend )

configure_file ( config.h.cmake config.h )
add_definitions ( -DHAVE_CONFIG_H )

### subdirs ###

add_subdirectory( src/ )
