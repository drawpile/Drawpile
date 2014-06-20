find_package(Qt5Network REQUIRED)

set (
	SOURCES
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
	record/writer.cpp
	record/reader.cpp
	util/idlist.cpp
	util/logger.cpp
	server/client.cpp
	server/session.cpp
	server/sessionserver.cpp
	server/sessiondesc.cpp
	server/sessionstore.cpp
	server/loginhandler.cpp
        server/opcommands.cpp
	)

add_library(${DPSHAREDLIB} STATIC ${SOURCES})

target_link_libraries(${DPSHAREDLIB} Qt5::Network)

# Shouldn't be needed anymore, but ubuntu 13.10 cmake doesn't work without this:  
qt5_use_modules(${DPSHAREDLIB} Network)

