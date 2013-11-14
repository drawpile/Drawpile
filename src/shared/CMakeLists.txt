# src/shared/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
find_package(Qt5Network)
include_directories( ${Qt5Network_INCLUDES} )

set (
	NET_HEADERS
	net/constants.h
	net/message.h
	net/annotation.h
	net/layer.h
	net/pen.h
	)

#set (
	#NET_QHEADERS
	#net/messagequeue.h
	#)

set (
	NET_SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/pen.cpp
	)

set (
	SRV_QHEADERS
	server/server.h
	)

set (
	SRV_SOURCES
	server/server.cpp
	)

add_library( ${DPSHAREDLIB} ${NET_SOURCES} ${SRV_SOURCES} )
target_link_libraries ( ${DPSHAREDLIB} ${Qt5Network_LIBRARIES} )

if (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION . )
else (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION lib )
endif (WIN32)

