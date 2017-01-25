# src/server/CMakeLists.txt

find_package( KF5Archive NO_MODULE )
find_package( Qt5Core )

set (
	DPREC2TXT_SOURCES
	dprec2txt.cpp
	)

set (
	TXT2DPREC_SOURCES
	txt2dprec.cpp
	)

add_executable( dprec2txt ${DPREC2TXT_SOURCES} )
target_link_libraries( dprec2txt ${DPSHAREDLIB} Qt5::Core)

add_executable( txt2dprec ${TXT2DPREC_SOURCES} )
target_link_libraries( txt2dprec ${DPSHAREDLIB} Qt5::Core)


if ( UNIX AND NOT APPLE )
	install ( TARGETS dprec2txt txt2dprec DESTINATION bin )
endif ()

