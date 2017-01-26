find_package( KF5Archive NO_MODULE )
find_package( Qt5Core )

set (
	DPRECTOOL_SOURCES
	dprectool.cpp
	)

add_executable( dprectool ${DPRECTOOL_SOURCES} )
target_link_libraries( dprectool ${DPSHAREDLIB} Qt5::Core)


if ( UNIX AND NOT APPLE )
	install ( TARGETS dprectool DESTINATION bin )
endif ()

