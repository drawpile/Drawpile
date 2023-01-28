add_library(dpshared STATIC
	qtshims.h
	listings/announcementapi.cpp
	listings/announcementapi.h
	listings/listserverfinder.cpp
	listings/listserverfinder.h
	net/annotation.cpp
	net/annotation.h
	net/brushes.cpp
	net/brushes.h
	net/control.cpp
	net/control.h
	net/image.cpp
	net/image.h
	net/layer.cpp
	net/layer.h
	net/message.cpp
	net/message.h
	net/messagequeue.cpp
	net/messagequeue.h
	net/meta.cpp
	net/meta.h
	net/meta2.cpp
	net/meta2.h
	net/opaque.cpp
	net/opaque.h
	net/protover.cpp
	net/protover.h
	net/recording.cpp
	net/recording.h
	net/textmode.cpp
	net/textmode.h
	net/undo.cpp
	net/undo.h
	record/header.cpp
	record/header.h
	record/reader.cpp
	record/reader.h
	record/writer.cpp
	record/writer.h
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
	util/ulid.cpp
	util/ulid.h
	util/validators.cpp
	util/validators.h
	util/whatismyip.cpp
	util/whatismyip.h
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
		${QT_PACKAGE_NAME}::AndroidExtras
	)
endif()

if(libsodium_FOUND)
	target_sources(dpshared PRIVATE
		util/authtoken.cpp
		util/authtoken.h
	)
	target_link_libraries(dpshared libsodium::libsodium)
endif()

if(KF5DNSSD_FOUND)
	target_sources(dpshared PRIVATE
		listings/zeroconfannouncement.cpp
		listings/zeroconfannouncement.h
		listings/zeroconfdiscovery.cpp
		listings/zeroconfdiscovery.h
	)
	target_link_libraries(dpshared KF5::DNSSD)
endif()
add_feature_info("Zeroconf support" KF5DNSSD_FOUND "")

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(dpshared)
