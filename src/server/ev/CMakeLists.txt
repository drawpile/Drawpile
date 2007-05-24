# src/server/

set ( LIBNAME "eventslib" )

# include some stuff

include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )
include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckVariableExists )

### Event mechanism

set ( EV_SRC "" )
set ( EVS "" )

if ( EPOLL )
	check_include_file ( sys/epoll.h EV_EPOLL )
	if ( EV_EPOLL )
		set ( EVS ${EVS} epoll )
		set ( EV_SRC ${EV_SRC} epoll.cpp )
		set ( EV_FOUND ON )
	endif ( EV_EPOLL )
endif ( EPOLL )

if ( KQUEUE )
	include ( CheckCSourceCompiles )
	check_c_source_compiles( "#include <sys/event.h>\nint main() { return kqueue(); }" EV_KQUEUE )
	if ( EV_KQUEUE )
		set ( EVS ${EVS} kqueue )
		set ( EV_SRC ${EV_SRC} kqueue.cpp )
		set ( EV_FOUND ON )
	endif ( EV_KQUEUE )
endif ( KQUEUE )

if ( PSELECT )
	check_function_exists ( pselect EV_PSELECT )
	if ( EV_PSELECT )
		set ( EVS ${EVS} pselect )
		set ( EV_SRC ${EV_SRC} pselect.cpp )
		set ( EV_FOUND ON )
	endif ( EV_PSELECT )
endif ( PSELECT )

# fall back to select(2)
if ( SELECT )
	check_include_file ( "sys/select.h" EV_SELECT )
	check_include_file ( winsock2.h HAVE_WINSOCK2 )
	if ( EV_SELECT )
		set ( EVS ${EVS} select )
		set ( EV_SRC ${EV_SRC} select.cpp )
		set ( EV_FOUND ON )
		check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" EV_SELECT_COPY )
	endif ( EV_SELECT )
	if ( HAVE_WINSOCK2 )
		set ( EV_SELECT ON )
		set ( EVS ${EVS} select )
		set ( EV_SRC ${EV_SRC} select.cpp )
		set ( EV_FOUND ON )
		check_symbol_exists ( FD_COPY "unistd.h" EV_SELECT_COPY )
		
		if ( WSA )
			set ( EVS ${EVS} wsa )
			set ( EV_SRC ${EV_SRC} wsa.cpp )
			set ( EV_WSA ON )
		endif ( WSA )
	endif ( HAVE_WINSOCK2 )
endif ( SELECT )

if ( NOT EV_FOUND )
	message ( FATAL_ERROR "No event mechanism found!" )
else ( NOT EV_FOUND)
	message ( STATUS "Event system/s: ${EVS}" )
endif ( NOT EV_FOUND )

# config

message ( STATUS ${CMAKE_CURRENT_SOURCE_DIR} )

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_EVCONFIG )

if ( NOT DEBUG )
	strip_exe ( ${LIBNAME} )
endif ( NOT DEBUG )

add_library (
	${LIBNAME}
	STATIC
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
