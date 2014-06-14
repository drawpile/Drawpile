# src/server/CMakeLists.txt

find_package( Qt5Network REQUIRED)
find_package( Libmicrohttpd )

set (
	SOURCES
	dpserver.cpp
	multiserver.cpp
	configfile.cpp
	sslserver.cpp
	hibernation.cpp
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

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} Qt5::Network ${INITSYS_LIB} ${MHD_LIBRARIES} )

# Shouldn't be needed anymore, but ubuntu 13.10 cmake doesn't work without this: 
qt5_use_modules( ${SRVNAME} Network )

if ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION . )
else ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ( WIN32 )

