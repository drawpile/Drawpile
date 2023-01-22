find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Network)

add_library(dpserver STATIC
	announcable.h
	announcements.cpp
	announcements.h
	client.cpp
	client.h
	filedhistory.cpp
	filedhistory.h
	idqueue.cpp
	idqueue.h
	inmemoryconfig.cpp
	inmemoryconfig.h
	inmemoryhistory.cpp
	inmemoryhistory.h
	jsonapi.cpp
	jsonapi.h
	loginhandler.cpp
	loginhandler.h
	opcommands.cpp
	opcommands.h
	serverconfig.cpp
	serverconfig.h
	serverlog.cpp
	serverlog.h
	session.cpp
	session.h
	sessionban.cpp
	sessionban.h
	sessionhistory.cpp
	sessionhistory.h
	sessions.h
	sessionserver.cpp
	sessionserver.h
	sslserver.cpp
	sslserver.h
	templateloader.h
	thinserverclient.cpp
	thinserverclient.h
	thinsession.cpp
	thinsession.h
)

target_link_libraries(dpserver
	dpshared
	${QT_PACKAGE_NAME}::Network
)

if(Sodium_FOUND)
	target_include_directories(dpserver SYSTEM PRIVATE ${SODIUM_INCLUDE_DIR})
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(dpserver)
