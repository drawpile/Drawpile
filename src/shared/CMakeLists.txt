find_package(Qt5Network REQUIRED)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(Sodium)

set (
	SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/brushes.cpp
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
	util/whatismyip.cpp
	util/networkaccess.cpp
	listings/announcementapi.cpp
	listings/announcements.cpp
	server/client.cpp
	server/thinserverclient.cpp
	server/session.cpp
	server/thinsession.cpp
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
	server/sslserver.cpp
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

if(LIBMINIUPNPC_FOUND)
	set ( SOURCES ${SOURCES} util/upnp.cpp )
endif()

if(KF5DNSSD_FOUND)
	set( SOURCES ${SOURCES}
		listings/zeroconfdiscovery.cpp
		listings/zeroconfannouncement.cpp
	)
endif()

add_library(${DPSHAREDLIB} STATIC ${SOURCES})

target_link_libraries(${DPSHAREDLIB} Qt5::Network)
target_link_libraries(${DPSHAREDLIB} KF5::Archive)

if( Sodium_FOUND )
	target_link_libraries(${DPSHAREDLIB} ${SODIUM_LIBRARY})
endif()

if(LIBMINIUPNPC_FOUND)
	target_link_libraries(${DPSHAREDLIB} ${LIBMINIUPNPC_LIBRARIES})
	if ( WIN32 )
		target_link_libraries (${DPSHAREDLIB} iphlpapi)
	endif ( WIN32 )
endif()

if(KF5DNSSD_FOUND)
	target_link_libraries(${DPSHAREDLIB} KF5::DNSSD)
endif()

if(TESTS)
	add_subdirectory(tests)
endif(TESTS)

