# src/server/CMakeLists.txt

find_package( Qt5Network REQUIRED)
find_package( Libmicrohttpd )
find_package( KF5Archive NO_MODULE )

set (
	SOURCES
	dpserver.cpp
	multiserver.cpp
	configfile.cpp
	sslserver.cpp
	hibernation.cpp
	updateablefile.cpp
	userfile.cpp
	announcementwhitelist.cpp
	)

# Unix specific features
if ( UNIX )
	set ( SOURCES ${SOURCES} unixsignals.cpp )
endif ( UNIX )

# Select init system integration backend
if ( INITSYS STREQUAL "systemd" AND SYSTEMD_FOUND )
	set ( SOURCES ${SOURCES} initsys_systemd.cpp )
	set ( INITSYS_LIB ${SYSTEMD_LIBRARIES} )
else ()
	set ( SOURCES ${SOURCES} initsys_dummy.cpp )
endif ()

# Enable web admin if libmicrohttpd was found
if( MHD_FOUND )
	set( SOURCES ${SOURCES}
		webadmin/qmhttp.cpp
		webadmin/webadmin.cpp
		webadmin/staticfileserver.cpp
		webadmin/api.cpp
	)
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_WEBADMIN" )
else( MHD_FOUND )
	message(WARNING "Libmicrohttpd not found: web-admin not enabled" )
endif( MHD_FOUND )

# Use local copy of Qt's command line parser for Qt <5.2
# Remove this when we no longer need to support old Qt.
if ( ${Qt5Network_VERSION_STRING} VERSION_LESS "5.2.0" )
	set ( SOURCES ${SOURCES} qcommandlineoption.cpp qcommandlineparser.cpp )
endif()

# Fall back to bundled KArchive if not found
if(NOT KF5Archive_FOUND)
        find_package (ZLIB REQUIRED)
        include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../shared/bundled/karchive")
endif()

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} Qt5::Network ${INITSYS_LIB} ${MHD_LIBRARIES} )

# Shouldn't be needed anymore, but ubuntu 13.10 cmake doesn't work without this: 
qt5_use_modules( ${SRVNAME} Network )

if(NOT KF5Archive_FOUND)
	target_link_libraries(${SRVNAME} ${ZLIB_LIBRARIES})
endif()

if ( UNIX AND NOT APPLE )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ()
