# src/server/

set (
	Server_Headers
	server.h
	user.h
	event.h
	sockets.h
	buffer.h
	session.h
)

set (
	Server_Sources
	dpsrv.cpp
	server.cpp
	sockets.cpp
)

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

### Boost library

find_package ( Boost 1.33 REQUIRED )

if ( Boost_FOUND )
	message ( STATUS "Boost C++ Libraries found" )
	include_directories( ${Boost_INCLUDE_DIRS} )
	link_directories( ${Boost_LIBRARY_DIRS} )
else ( Boost_FOUND )
	message ( FATAL_ERROR "Boost C++ Libraries missing" )
endif ( Boost_FOUND )

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
				if ( EV_SELECT_COPY )
					message( STATUS "FD_COPY found" )
				endif ( EV_SELECT_COPY )
			endif ( NOT WIN32 )
		endif ( EV_SELECT )
	endif ( SELECT )
endif ( NOT EV_FOUND )

if ( WIN32 )
	check_include_file ( winsock2.h HAVE_WSA )
	if ( HAVE_WSA )
		message( STATUS "Windows Socket API found")
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

### GNU Extensions

if ( GNUEXT )
	check_include_file_cxx ( ext/slist HAVE_SLIST )
	check_include_file_cxx ( ext/hash_set HAVE_HASH_SET )
	check_include_file_cxx ( ext/hash_map HAVE_HASH_MAP )
endif ( GNUEXT )

# config

configure_file ( config.h.cmake ${CMAKE_SOURCE_DIR}/src/server/config.h )
add_definitions ( -DHAVE_SRVCONFIG )

### link

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

link_libraries( ${LIBNAME} ) # DP shared library

if ( NOT DEBUG )
	strip_exe ( ${SERVERNAME} )
endif ( NOT DEBUG )

add_executable ( ${SERVERNAME} MACOSX_BUNDLE ${Server_Sources} ${EV_SRC} ${Server_Headers} )

install ( TARGETS ${SERVERNAME} RUNTIME DESTINATION bin )
