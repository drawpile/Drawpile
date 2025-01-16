add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_library(dpserver)

target_sources(dpserver PRIVATE
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
	PRIVATE
		cmake-config
	PUBLIC
		dpshared
		${QT_PACKAGE_NAME}::Core
		${QT_PACKAGE_NAME}::Network
)

if(TARGET "${QT_PACKAGE_NAME}::WebSockets")
	target_link_libraries(dpserver PUBLIC ${QT_PACKAGE_NAME}::WebSockets)
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

directory_auto_source_groups()
