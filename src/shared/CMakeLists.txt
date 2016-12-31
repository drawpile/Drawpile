find_package(Qt5Network REQUIRED)
find_package(KF5Archive REQUIRED NO_MODULE)

set (
	SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/pen.cpp
	net/image.cpp
	net/annotation.cpp
	net/control.cpp
	net/meta.cpp
	net/meta2.cpp
	net/opaque.cpp
	net/undo.cpp
	net/recording.cpp
	net/messagequeue.cpp
	net/messagestream.cpp
	net/protover.cpp
	record/writer.cpp
	record/reader.cpp
	util/logger.cpp
	util/passwordhash.cpp
	util/filename.cpp
	util/announcementapi.cpp
	server/client.cpp
	server/session.cpp
	server/sessionserver.cpp
	server/loginhandler.cpp
	server/opcommands.cpp
	server/serverconfig.cpp
	server/jsonapi.cpp
	)

add_library(${DPSHAREDLIB} STATIC ${SOURCES})

target_link_libraries(${DPSHAREDLIB} Qt5::Network)
target_link_libraries(${DPSHAREDLIB} KF5::Archive)

