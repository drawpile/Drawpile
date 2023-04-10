add_library(dpshared)

target_sources(dpshared PRIVATE
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
	util/qtcompat.h
	util/ulid.cpp
	util/ulid.h
	util/validators.cpp
	util/validators.h
	util/whatismyip.cpp
	util/whatismyip.h
)

target_link_libraries(dpshared
	PRIVATE
		cmake-config
	PUBLIC
		${QT_PACKAGE_NAME}::Core
		${QT_PACKAGE_NAME}::Network
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

if(TARGET KF5::DNSSD)
	target_sources(dpshared PRIVATE
		listings/zeroconfannouncement.cpp
		listings/zeroconfannouncement.h
		listings/zeroconfdiscovery.cpp
		listings/zeroconfdiscovery.h
	)
	target_link_libraries(dpshared PUBLIC KF5::DNSSD)
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

directory_auto_source_groups()
