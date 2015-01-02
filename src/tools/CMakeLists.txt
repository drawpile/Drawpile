# src/server/CMakeLists.txt

find_package( KF5Archive NO_MODULE )
find_package( Qt5Core )

set (
	DPREC2TXT_SOURCES
	dprec2txt.cpp
	txtmsg.cpp
	../client/core/rasterop.cpp # needed for svgBlendMode
	)

# Fall back to bundled KArchive if not found
if(NOT KF5Archive_FOUND)
        find_package (ZLIB REQUIRED)
        include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../shared/bundled/karchive")
endif()

add_executable( dprec2txt ${DPREC2TXT_SOURCES} )
target_link_libraries( dprec2txt ${DPSHAREDLIB} Qt5::Core)

if(NOT KF5Archive_FOUND)
	target_link_libraries(dprec2txt ${ZLIB_LIBRARIES})
endif()

if ( UNIX AND NOT APPLE )
	install ( TARGETS dprec2txt DESTINATION bin )
endif ()

