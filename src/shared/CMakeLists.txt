# src/shared/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
find_package(Qt5Network)
include_directories( ${Qt5Network_INCLUDES} )

set (
	NET_SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/pen.cpp
	net/image.cpp
	net/annotation.cpp
	)

set (
	UTIL_SOURCES
	util/idlist.cpp
	)

set (
	SRV_SOURCES
	server/server.cpp
	)

add_library( ${DPSHAREDLIB} ${NET_SOURCES} ${UTIL_SOURCES} ${SRV_SOURCES} )
target_link_libraries ( ${DPSHAREDLIB} ${Qt5Network_LIBRARIES} )

if (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION . )
else (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION lib )
endif (WIN32)

