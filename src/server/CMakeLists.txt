set ( SERVER_HEADERS user.h event.h buffer.h )

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

link_libraries( dpshared )

add_executable ( dpsrv dpsrv.cpp event.cpp ${SERVER_HEADERS} )

