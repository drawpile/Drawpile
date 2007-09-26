# src/server/ev/

set ( EV_FOUND OFF )
set ( EV_SRC "" )
set ( EVS "" )
set ( EV_LIBRARY "" )

include ( CheckIncludeFile )
include ( CheckSymbolExists )

### Event mechanism

set ( EV_EPOLL OFF )
if ( NOT EV_FOUND AND EPOLL )
	check_include_file ( sys/epoll.h EV_EPOLL )
	if ( EV_EPOLL )
		set ( EVS "epoll(4)" )
		set ( EV_SRC epoll.cpp )
		set ( EV_FOUND ON )
	endif ( EV_EPOLL )
endif ( NOT EV_FOUND AND EPOLL )

set ( EV_KQUEUE OFF )
if ( NOT EV_FOUND AND KQUEUE )
	include ( CheckCSourceCompiles )
	check_c_source_compiles( "#include <sys/event.h>\nint main() { return kqueue(); }" EV_KQUEUE )
	if ( EV_KQUEUE )
		set ( EVS "kqueue(2)" )
		set ( EV_SRC kqueue.cpp )
		set ( EV_FOUND ON )
	endif ( EV_KQUEUE )
endif ( NOT EV_FOUND AND KQUEUE )

set ( EV_WSAPOLL OFF )
if ( NOT EV_FOUND AND WSAPOLL )
	include ( CheckCSourceCompiles )
	# Ws2_32.lib
	check_c_source_compiles( "#include <winsock2.h>\nint main() { WSAPOLLFD stub[1]; return WSAPoll(stub, 1, 0); }" EV_WSAPOLL )
	if ( EV_WSAPOLL )
		set ( EVS "WSAPoll" )
		set ( EV_SRC wsapoll.cpp )
		set ( EV_FOUND ON )
	endif ( EV_WSAPOLL )
endif ( NOT EV_FOUND AND WSAPOLL )

# fall back to select(2)
set ( EV_SELECT OFF )
if ( NOT EV_FOUND AND SELECT )
	if ( WIN32 )
		check_include_file ( "winsock2.h" HAVE_WINSOCK2 )
	else ( WIN32 )
		check_include_file ( "sys/select.h" HAVE_SELECT_H )
	endif ( WIN32 )
	
	if ( HAVE_SELECT_H OR HAVE_WINSOCK2 )
		set ( EV_SELECT ON )
		set ( EVS "select(2)" )
		set ( EV_SRC select.cpp )
		set ( EV_FOUND ON )
		check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" EV_SELECT_COPY )
	endif ( HAVE_SELECT_H OR HAVE_WINSOCK2 )
endif ( NOT EV_FOUND AND SELECT )

if ( NOT EV_FOUND )
	message ( FATAL_ERROR "No event mechanism found!" )
else ( NOT EV_FOUND)
	message ( STATUS "Event system: ${EVS}" )
endif ( NOT EV_FOUND )

# config

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_EVCONFIG )

set (
	EV_SOURCES
	${EV_SRC}
)

if ( RELEASE )
	InSourceDir ( EV_SOURCES ${EV_SOURCES} )
	generate_final ( EV_SOURCES ${EV_SOURCES} )
endif ( RELEASE )

add_library (
	${EVENTSLIB}
	STATIC
	${EV_SOURCES}
)

set_target_properties (
	${EVENTSLIB}
	PROPERTIES
	OUTPUT_NAME events
	VERSION 0.1.0
	SOVERSION 0.1.0
	PROJECT_LABEL eventslibrary
)
