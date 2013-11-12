# src/server/CMakeLists.txt

find_package( Qt5Network )
include_directories( ${Qt5Network_INCLUDES} )
add_definitions(${Qt5Network_DEFINITIONS})

set (
	SOURCES
	dpserver.cpp
	)

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} ${Qt5Network_LIBRARIES} )

if ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION . )
else ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ( WIN32 )

