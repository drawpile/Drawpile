find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Sql)

add_library(dpshared)

target_sources(dpshared PRIVATE
	listings/announcementapi.cpp
	listings/announcementapi.h
	listings/listserverfinder.cpp
	listings/listserverfinder.h
	net/message.cpp
	net/message.h
	net/messagequeue.cpp
	net/messagequeue.h
	net/protover.cpp
	net/protover.h
	net/servercmd.cpp
	net/servercmd.h
	util/database.cpp
	util/database.h
	util/filename.cpp
	util/filename.h
	util/functionrunnable.cpp
	util/functionrunnable.h
	util/networkaccess.cpp
	util/networkaccess.h
	util/passwordhash.cpp
	util/passwordhash.h
	util/paths.cpp
	util/paths.h
	util/qtcompat.h
	util/ulid.cpp
	util/ulid.h
	util/validators.cpp
	util/validators.h
	util/whatismyip.cpp
	util/whatismyip.h
)

if(HAVE_TCPSOCKETS)
	target_sources(dpshared PRIVATE
		net/tcpmessagequeue.cpp
		net/tcpmessagequeue.h
	)
endif()

if(NOT EMSCRIPTEN)
	target_compile_definitions(dpshared PUBLIC HAVE_COMPATIBILITY_MODE=1)
endif()

target_link_libraries(dpshared
	PRIVATE
		cmake-config
	PUBLIC
		drawdance_server
		${QT_PACKAGE_NAME}::Core
		${QT_PACKAGE_NAME}::Network
		${QT_PACKAGE_NAME}::Sql
)

if(ANDROID)
	target_sources(dpshared PRIVATE
		util/androidutils.cpp
		util/androidutils.h
	)

	if(TARGET ${QT_PACKAGE_NAME}::AndroidExtras)
		target_link_libraries(dpshared PUBLIC ${QT_PACKAGE_NAME}::AndroidExtras)
	endif()
endif()

if(TARGET libsodium::libsodium)
	target_sources(dpshared PRIVATE
		util/authtoken.cpp
		util/authtoken.h
	)
	target_link_libraries(dpshared PUBLIC libsodium::libsodium)
endif()

if(TARGET ${QT_PACKAGE_NAME}::WebSockets)
	target_sources(dpshared PRIVATE
		net/websocketmessagequeue.cpp
		net/websocketmessagequeue.h
	)
	target_link_libraries(dpshared PUBLIC ${QT_PACKAGE_NAME}::WebSockets)
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

directory_auto_source_groups()
