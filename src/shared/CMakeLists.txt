find_package(Qt5Network REQUIRED)
find_package(KF5Archive NO_MODULE)

set (
	SOURCES
	net/message.cpp
	net/annotation.cpp
	net/layer.cpp
	net/pen.cpp
	net/image.cpp
	net/annotation.cpp
	net/login.cpp
	net/snapshot.cpp
	net/meta.cpp
	net/flow.cpp
	net/undo.cpp
	net/recording.cpp
	net/messagequeue.cpp
	net/messagestream.cpp
	record/writer.cpp
	record/reader.cpp
	util/idlist.cpp
	util/logger.cpp
	util/passwordhash.cpp
	util/filename.cpp
	server/client.cpp
	server/session.cpp
	server/sessionserver.cpp
	server/sessiondesc.cpp
	server/sessionstore.cpp
	server/loginhandler.cpp
	server/opcommands.cpp
	server/identitymanager.cpp
	)

# Use bundled KArchive if library not found
if(NOT KF5Archive_FOUND)
	message(WARNING "KArchive not found: using bundled copy!")
	find_package (ZLIB REQUIRED)

	set(
		SOURCES ${SOURCES}
		bundled/karchive/karchive.cpp
		bundled/karchive/klimitediodevice.cpp
		bundled/karchive/kzip.cpp
		bundled/karchive/kcompressiondevice.cpp
		bundled/karchive/kfilterbase.cpp
		bundled/karchive/kfilterdev.cpp
		bundled/karchive/klimitediodevice.cpp
		bundled/karchive/kgzipfilter.cpp
		bundled/karchive/knonefilter.cpp
	)

	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/bundled/karchive")
	#target_link_libraries(${DPSHAREDLIB} ${ZLIB_LIBRARIES})
else()
	#target_link_libraries(${DPSHAREDLIB} KF5::Archive)
endif()

add_library(${DPSHAREDLIB} STATIC ${SOURCES})

target_link_libraries(${DPSHAREDLIB} Qt5::Network)

if(NOT KF5Archive_FOUND)
	target_link_libraries(${DPSHAREDLIB} ${ZLIB_LIBRARIES})
else()
	target_link_libraries(${DPSHAREDLIB} KF5::Archive)
endif()


# Shouldn't be needed anymore, but ubuntu 13.10 cmake doesn't work without this:  
qt5_use_modules(${DPSHAREDLIB} Network)

