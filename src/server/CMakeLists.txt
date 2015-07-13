# src/server/CMakeLists.txt

find_package( Qt5Network REQUIRED)
find_package( Libmicrohttpd )
find_package( KF5Archive REQUIRED NO_MODULE )

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
	banlist.cpp
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

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} Qt5::Network ${INITSYS_LIB} ${MHD_LIBRARIES} )

if ( UNIX AND NOT APPLE )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ()
