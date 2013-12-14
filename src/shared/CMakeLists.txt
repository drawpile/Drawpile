# src/shared/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
find_package(Qt5Network)

set (
	NET_SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/pen.cpp
	net/image.cpp
	net/annotation.cpp
	net/login.cpp
	net/snapshot.cpp
	net/meta.cpp
	net/undo.cpp
	net/messagequeue.cpp
	net/messagestream.cpp
	)

set (
	UTIL_SOURCES
	util/idlist.cpp
	)

set (
	SRV_SOURCES
	server/server.cpp
	server/client.cpp
	server/session.cpp
	)

add_library( ${DPSHAREDLIB} STATIC ${NET_SOURCES} ${UTIL_SOURCES} ${SRV_SOURCES} )
target_link_libraries ( ${DPSHAREDLIB} ${Qt5Network_LIBRARIES} )
qt5_use_modules( ${DPSHAREDLIB} Network )

