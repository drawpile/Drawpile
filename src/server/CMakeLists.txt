# src/server/CMakeLists.txt

find_package( Qt4 REQUIRED QtCore QtNetwork )
include ( ${QT_USE_FILE} )

set (
	SOURCES
	dpserver.cpp
	)

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} ${QT_LIBRARIES} )

if ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION . )
else ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ( WIN32 )

