find_package(Qt5Network REQUIRED)
find_package(Sodium)

set (
	SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/brushes.cpp
	net/image.cpp
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
	util/validators.cpp
	util/paths.cpp
	util/ulid.cpp
	listings/announcementapi.cpp
	listings/listserverfinder.cpp
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

if(KF5DNSSD_FOUND)
	set( SOURCES ${SOURCES}
		listings/zeroconfdiscovery.cpp
		listings/zeroconfannouncement.cpp
	)
endif()

add_library(dpshared STATIC ${SOURCES})

target_link_libraries(dpshared Qt5::Network)

if( Sodium_FOUND )
	target_link_libraries(dpshared ${SODIUM_LIBRARY})
endif()

if(KF5DNSSD_FOUND)
	target_link_libraries(dpshared KF5::DNSSD)
endif()

if(TESTS)
	add_subdirectory(tests)
endif(TESTS)

