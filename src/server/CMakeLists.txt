# src/server/

set (
	Server_Headers
	server.h
	user.h
	event.h
	sockets.h
	buffer.h
)

set (
	Server_Sources
	dpsrv.cpp
	server.cpp
	event.cpp
	sockets.cpp
)

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

include ( AddFileDependencies )
add_file_dependencies (
	dpsrv
	shared
)

link_libraries( dpshared )

add_executable ( dpsrv ${Server_Sources} ${Server_Headers} )
