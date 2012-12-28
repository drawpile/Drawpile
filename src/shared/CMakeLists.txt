# src/shared/CMakeLists.txt

find_package(Qt4 REQUIRED QtCore QtNetwork)
include ( ${QT_USE_FILE} )

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

QT4_WRAP_CPP (MOC_Sources ${NET_QHEADERS} ${SRV_QHEADERS} )

add_library( ${DPSHAREDLIB} ${NET_SOURCES} ${SRV_SOURCES} ${MOC_Sources} )
target_link_libraries ( ${DPSHAREDLIB} ${QT_LIBRARIES} )

if (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION . )
else (WIN32)
	install ( TARGETS ${DPSHAREDLIB} DESTINATION lib )
endif (WIN32)

