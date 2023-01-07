find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Network)
find_package(Sodium QUIET)

add_library(dpserver STATIC
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
	announcements.cpp
)

target_link_libraries(dpserver
	dpshared
	${QT_PACKAGE_NAME}::Network
)

if(Sodium_FOUND)
	target_compile_definitions(dpserver PRIVATE HAVE_LIBSODIUM)
	target_include_directories(dpserver SYSTEM PRIVATE ${SODIUM_INCLUDE_DIR})
endif()
add_feature_info("Ext-auth support" Sodium_FOUND "")

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(dpserver)
