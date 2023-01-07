add_library(dpshared STATIC
	net/message.cpp
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
	util/functionrunnable.cpp
	util/networkaccess.cpp
	util/passwordhash.cpp
	util/paths.cpp
	util/ulid.cpp
	util/validators.cpp
	util/whatismyip.cpp
	listings/announcementapi.cpp
	listings/listserverfinder.cpp
)

target_link_libraries(dpshared
	${QT_PACKAGE_NAME}::Network
)

if(ANDROID)
	target_sources(dpshared PRIVATE
		util/androidutils.cpp
		util/androidutils.h
	)
	target_link_libraries(dpshared PRIVATE
		${QT_PACKAGE_NAME}::QtAndroidExtras
	)
endif()

find_package(Sodium)
if(Sodium_FOUND)
	target_sources(dpshared PRIVATE util/authtoken.cpp)
	target_compile_definitions(dpshared PRIVATE HAVE_LIBSODIUM)
	target_include_directories(dpshared SYSTEM PRIVATE ${SODIUM_INCLUDE_DIR})
	target_link_libraries(dpshared ${SODIUM_LIBRARY})
endif()
add_feature_info("Ext-auth support" Sodium_FOUND "")

if(KF5DNSSD_FOUND)
	message(STATUS "KF5 DNSSD enabled")
	target_sources(dpshared
		listings/zeroconfdiscovery.cpp
		listings/zeroconfannouncement.cpp
	)
	target_link_libraries(dpshared KF5::DNSSD)
endif()
add_feature_info("Zeroconf support" KF5DNSSD_FOUND "")

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(dpshared)
