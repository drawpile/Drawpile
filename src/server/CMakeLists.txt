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

### Event mechanism sources.

set ( EV_SRC event.select.cpp )

if ( EV_WSA )
	set ( EV_SRC event.select.cpp )
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

### link

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

link_libraries( dpshared )

if (MACOSX)
	set ( EXEFORMAT MACOSX_BUNDLE )
endif (MACOSX)

if ( NOT DEBUG )
	strip_exe ( dpserver )
endif ( NOT DEBUG )

add_executable ( dpserver ${EXEFORMAT} ${Server_Sources} ${EV_SRC} ${Server_Headers} )
