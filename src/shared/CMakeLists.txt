find_package(Qt5Network REQUIRED)

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
	net/flow.cpp
	net/undo.cpp
	net/recording.cpp
	net/messagequeue.cpp
	net/messagestream.cpp
	)

set (
	RECORDING_SOURCES
	record/writer.cpp
	record/reader.cpp
	)

set (
	UTIL_SOURCES
	util/idlist.cpp
	util/logger.cpp
	)

set (
	SRV_SOURCES
	server/client.cpp
	server/session.cpp
	server/sessionserver.cpp
	server/loginhandler.cpp
	)

add_library( ${DPSHAREDLIB} STATIC ${NET_SOURCES} ${RECORDING_SOURCES} ${UTIL_SOURCES} ${SRV_SOURCES} )

target_link_libraries ( ${DPSHAREDLIB} Qt5::Network )
