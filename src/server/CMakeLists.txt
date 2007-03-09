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
	server.mainloop.cpp
	sockets.cpp
)

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

### link

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

link_libraries( ${LIBNAME} )

# zlib for deflate extensions
if ( HAVE_ZLIB )
	link_libraries ( "z" )
endif ( HAVE_ZLIB )

if ( NOT DEBUG )
	strip_exe ( ${SERVERNAME} )
endif ( NOT DEBUG )

add_executable ( ${SERVERNAME} MACOSX_BUNDLE ${Server_Sources} ${EV_SRC} ${Server_Headers} )
