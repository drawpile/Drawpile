find_package(Qt5Network REQUIRED)
find_package(Sodium)

set (
	SOURCES
	client.cpp
	thinserverclient.cpp
	session.cpp
	thinsession.cpp
	sessionserver.cpp
	sessionban.cpp
	sessionhistory.cpp
	inmemoryhistory.cpp
	filedhistory.cpp
	loginhandler.cpp
	opcommands.cpp
	serverconfig.cpp
	inmemoryconfig.cpp
	jsonapi.cpp
	idqueue.cpp
	serverlog.cpp
	sslserver.cpp
	announcements.cpp
	)

if( Sodium_FOUND )
	add_definitions(-DHAVE_LIBSODIUM)
	include_directories(system "${SODIUM_INCLUDE_DIR}")
else( Sodium_FOUND )
	message(WARNING "Libsodium not found: Ext-auth support not enabled" )
endif( Sodium_FOUND )

add_library(dpserver STATIC ${SOURCES})
target_link_libraries(dpserver dpshared Qt5::Network)

if(TESTS)
	add_subdirectory(tests)
endif(TESTS)

