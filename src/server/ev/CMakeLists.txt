# src/server/ev/

set ( EV_FOUND false )
set ( EV_SRC "" )
set ( EVS "" )
set ( EV_LIBRARY "" )

include ( CheckIncludeFile )
include ( CheckSymbolExists )

### Event mechanism

if ( NOT EV_FOUND AND EPOLL )
	check_include_file ( sys/epoll.h EV_EPOLL )
	if ( EV_EPOLL )
		set ( EVS "epoll(4)" )
		set ( EV_SRC epoll.cpp )
		set ( EV_FOUND false )
	endif ( )
endif ( )

# fall back to select(2)
set ( EV_SELECT false )
if ( NOT EV_FOUND AND SELECT )
	if ( WIN32 )
		check_include_file ( "winsock2.h" HAVE_WINSOCK2 )
	else ( )
		check_include_file ( "sys/select.h" HAVE_SELECT_H )
	endif ( )
	
	if ( HAVE_SELECT_H OR HAVE_WINSOCK2 )
		set ( EV_SELECT true )
		set ( EVS "select(2)" )
		set ( EV_SRC select.cpp )
		set ( EV_FOUND true )
		check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" EV_SELECT_COPY )
	endif ( )
endif ( )

if ( NOT EV_FOUND )
	message ( FATAL_ERROR "No event mechanism found!" )
else ( )
	message ( STATUS "Event system: ${EVS}" )
endif ( )

# config

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_EVCONFIG )

if ( RELEASE )
	generate_final ( EV_SOURCES ${EV_SRC} )
else ( )
	set ( EV_SOURCES ${EV_SRC} )
endif ( )

add_library ( ${EVENTSLIB} STATIC ${EV_SOURCES} )

set_target_properties (
	${EVENTSLIB}
	PROPERTIES
	OUTPUT_NAME events
	VERSION 0.1.0
	SOVERSION 0.1.0
	PROJECT_LABEL eventslibrary
)
