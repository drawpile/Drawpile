set(libshared_sources
	listings/announcementapi.cpp
	listings/listserverfinder.cpp
	net/annotation.cpp
	net/brushes.cpp
	net/control.cpp
	net/image.cpp
	net/layer.cpp
	net/message.cpp
	net/messagequeue.cpp
	net/meta.cpp
	net/meta2.cpp
	net/opaque.cpp
	net/protover.cpp
	net/recording.cpp
	net/textmode.cpp
	net/undo.cpp
	record/header.cpp
	record/reader.cpp
	record/writer.cpp
	util/filename.cpp
	util/networkaccess.cpp
	util/passwordhash.cpp
	util/paths.cpp
	util/ulid.cpp
	util/validators.cpp
	util/whatismyip.cpp
)

add_library(dpshared STATIC ${libshared_sources})
set_drawpile_target_properties(dpshared)
target_link_libraries(dpshared PUBLIC Qt5::Network)

if(Sodium_FOUND)
	target_sources(dpshared PRIVATE util/authtoken.cpp)
	target_compile_definitions(dpshared PUBLIC HAVE_LIBSODIUM)
	target_include_directories(dpshared SYSTEM PUBLIC "${SODIUM_INCLUDE_DIR}")
	target_link_libraries(dpshared PUBLIC ${SODIUM_LIBRARY})
else()
	message(WARNING "Libsodium not found: Ext-auth support not enabled" )
endif()

if(KF5DNSSD_FOUND)
	target_sources(dpshared PRIVATE
		listings/zeroconfdiscovery.cpp listings/zeroconfannouncement.cpp)
	target_link_libraries(dpshared PUBLIC KF5::DNSSD)
endif()

if(TESTS)
	add_subdirectory(tests)
endif()
