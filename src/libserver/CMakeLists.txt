set(libserver_sources
	announcements.cpp
	client.cpp
	filedhistory.cpp
	idqueue.cpp
	inmemoryconfig.cpp
	inmemoryhistory.cpp
	jsonapi.cpp
	loginhandler.cpp
	opcommands.cpp
	serverconfig.cpp
	serverlog.cpp
	session.cpp
	sessionban.cpp
	sessionhistory.cpp
	sessionserver.cpp
	sslserver.cpp
	thinserverclient.cpp
	thinsession.cpp
)

add_library(dpserver STATIC ${libserver_sources})
set_drawpile_target_properties(dpserver)
target_link_libraries(dpserver dpshared DP::QtNetwork)

if(TESTS)
	add_subdirectory(tests)
endif()

