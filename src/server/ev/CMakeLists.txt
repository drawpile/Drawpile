# src/server/

set ( LIBNAME "eventslib" )

# include some stuff

include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )
include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckVariableExists )

### Event mechanism

if ( EPOLL )
	check_include_file ( sys/epoll.h EV_EPOLL )
endif ( EPOLL )

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

if ( PSELECT )
	check_function_exists ( pselect EV_PSELECT )
	if ( EV_PSELECT )
		set ( EV_FOUND ON )
	endif ( EV_PSELECT )
endif ( PSELECT )

# fall back to select(2)
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

if ( WIN32 )
	check_include_file ( winsock2.h HAVE_WSA )
	if ( HAVE_WSA )
		message( STATUS "Windows Socket API found" )
		if ( WSA )
			set ( EV_WSA ON )
			set ( EV_FOUND ON )
		else ( WSA )
			set ( EV_SELECT ON )
			set ( EV_FOUND ON )
		endif ( WSA )
	endif ( HAVE_WSA )
endif ( WIN32 )

if ( NOT HAVE_WSA )
	message ( STATUS "Using POSIX Socket API" )
endif ( NOT HAVE_WSA )

if ( NOT EV_FOUND )
	status ( FATAL_ERROR "No event mechanism found!" )
endif ( NOT EV_FOUND )

### Event mechanism sources.

set ( EV_SRC "" )

if ( EV_SELECT )
	set ( EV_SRC ${EV_SRC} select.cpp )
endif ( EV_SELECT )

if ( EV_PSELECT )
	set ( EV_SRC ${EV_SRC} pselect.cpp )
endif ( EV_PSELECT )

if ( EV_WSA )
	set ( EV_SRC ${EV_SRC} wsa.cpp )
endif ( EV_WSA )

if ( EV_KQUEUE )
	set ( EV_SRC ${EV_SRC} kqueue.cpp )
endif ( EV_KQUEUE )

if ( EV_EPOLL )
	set ( EV_SRC ${EV_SRC} epoll.cpp )
endif ( EV_EPOLL )

if ( EV_KEVENT )
	set ( EV_SRC ${EV_SRC} kevent.cpp )
endif ( EV_KEVENT )

# config

message ( STATUS ${CMAKE_CURRENT_SOURCE_DIR} )

configure_file ( config.h.cmake ${CMAKE_SOURCE_DIR}/src/server/ev/config.h )
add_definitions ( -DHAVE_EVCONFIG )

if ( NOT DEBUG )
	strip_exe ( ${LIBNAME} )
endif ( NOT DEBUG )

add_library (
	${LIBNAME}
	STATIC
	event.h
	interface.h
	traits.h
	${EV_SRC}
)

set_target_properties (
	${LIBNAME}
	PROPERTIES
	OUTPUT_NAME events
	VERSION 0.0.1
	SOVERSION 0.0.1
	PROJECT_LABEL eventslibrary
)
