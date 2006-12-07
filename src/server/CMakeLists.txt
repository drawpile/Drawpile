# src/server/

set (
	Server_Headers
	user.h
	event.h
	buffer.h
)

set (
	Server_Sources
	dpsrv.cpp
	event.cpp
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
