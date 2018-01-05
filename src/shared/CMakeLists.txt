find_package(Qt5Network REQUIRED)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(Sodium)

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
	net/protover.cpp
	net/textmode.cpp
	record/writer.cpp
	record/reader.cpp
	record/header.cpp
	util/passwordhash.cpp
	util/filename.cpp
	util/announcementapi.cpp
	util/whatismyip.cpp
	util/networkaccess.cpp
	server/client.cpp
	server/session.cpp
	server/sessionserver.cpp
	server/sessionban.cpp
	server/sessionhistory.cpp
	server/inmemoryhistory.cpp
	server/filedhistory.cpp
	server/loginhandler.cpp
	server/opcommands.cpp
	server/serverconfig.cpp
	server/inmemoryconfig.cpp
	server/jsonapi.cpp
	server/idqueue.cpp
	server/serverlog.cpp
	)

if( Sodium_FOUND )
	set( SOURCES ${SOURCES}
		util/authtoken.cpp
	)
	add_definitions(-DHAVE_LIBSODIUM)
	include_directories(system "${SODIUM_INCLUDE_DIR}")
else( Sodium_FOUND )
	message(WARNING "Libsodium not found: Ext-auth support not enabled" )
endif( Sodium_FOUND )

add_library(${DPSHAREDLIB} STATIC ${SOURCES})

target_link_libraries(${DPSHAREDLIB} Qt5::Network)
target_link_libraries(${DPSHAREDLIB} KF5::Archive)

if( Sodium_FOUND )
	target_link_libraries(${DPSHAREDLIB} ${SODIUM_LIBRARY})
endif()

if(TESTS)
	add_subdirectory(tests)
endif(TESTS)

