# src/server/ev/

# include some stuff
include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )
include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckVariableExists )

### Event mechanism

set ( EV_SRC "" )
set ( EVS "" )

set ( EV_EPOLL OFF )
if ( EPOLL )
	check_include_file ( sys/epoll.h EV_EPOLL )
	if ( EV_EPOLL )
		set ( EVS ${EVS} epoll )
		set ( EV_SRC ${EV_SRC} epoll.cpp )
		set ( EV_FOUND ON )
	endif ( EV_EPOLL )
endif ( EPOLL )

set ( EV_KQUEUE OFF )
if ( KQUEUE )
	include ( CheckCSourceCompiles )
	check_c_source_compiles( "#include <sys/event.h>\nint main() { return kqueue(); }" EV_KQUEUE )
	if ( EV_KQUEUE )
		set ( EVS ${EVS} kqueue )
		set ( EV_SRC ${EV_SRC} kqueue.cpp )
		set ( EV_FOUND ON )
	endif ( EV_KQUEUE )
endif ( KQUEUE )

set ( EV_WSAPOLL OFF )
if ( WSAPOLL )
	include ( CheckCSourceCompiles )
	# Ws2_32.lib
	check_c_source_compiles( "#include <winsock2.h>\nint main() { WSAPOLLFD stub[1]; return WSAPoll(stub, 1, 0); }" EV_WSAPOLL )
	if ( EV_WSAPOLL )
		set ( EVS ${EVS} WSAPoll )
		set ( EV_SRC ${EV_SRC} wsapoll.cpp )
		set ( EV_FOUND ON )
	endif ( EV_WSAPOLL )
endif ( WSAPOLL )

# fall back to select(2)
set ( EV_SELECT OFF )
if ( NOT EV_FOUND AND SELECT )
	if ( WIN32 )
		check_include_file ( "winsock2.h" HAVE_WINSOCK2 )
	else ( WIN32 )
		check_include_file ( "sys/select.h" EV_SELECT )
	endif ( WIN32 )
	
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
	endif ( HAVE_WINSOCK2 )
endif ( NOT EV_FOUND AND SELECT )

if ( NOT EV_FOUND )
	message ( FATAL_ERROR "No event mechanism found!" )
else ( NOT EV_FOUND)
	message ( STATUS "Event systems: ${EVS}" )
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
