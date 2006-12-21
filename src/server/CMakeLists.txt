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
	src/shared
)

link_libraries( dpshared )

if (MACOSX)
	set ( EXEFORMAT MACOSX_BUNDLE )
endif (MACOSX)

add_executable ( dpserver ${EXEFORMAT} ${Server_Sources} ${Server_Headers} )
