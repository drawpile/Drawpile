# src/shared/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
find_package(Qt5Network)
include_directories( ${Qt5Network_INCLUDES} )

set (
	NET_HEADERS
	net/binary.h net/login.h net/message.h net/protocol.h net/constants.h
	net/stroke.h net/toolselect.h net/utils.h net/annotation.h
	)

set (
	NET_QHEADERS
	net/messagequeue.h
	)

set (
	NET_SOURCES
	net/login.cpp net/message.cpp net/protocol.cpp net/stroke.cpp
	net/toolselect.cpp net/messagequeue.cpp net/utils.cpp net/annotation.cpp
	)

set (
	SRV_QHEADERS
	server/server.h server/client.h server/board.h
	)

set (
	SRV_SOURCES
	server/server.cpp server/client.cpp server/board.cpp
	)

add_library( ${DPSHAREDLIB} ${NET_SOURCES} ${SRV_SOURCES} )
target_link_libraries ( ${DPSHAREDLIB} ${Qt5Network_LIBRARIES} )

if (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION . )
else (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION lib )
endif (WIN32)

