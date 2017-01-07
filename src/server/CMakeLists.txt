# src/server/CMakeLists.txt

find_package( Qt5Network REQUIRED)
find_package( Qt5Sql REQUIRED)
find_package( Libmicrohttpd )
find_package( KF5Archive REQUIRED NO_MODULE )

set (
	SOURCES
	main.cpp
	multiserver.cpp
	sslserver.cpp
	database.cpp
	headless/headless.cpp
	)

if(SERVERGUI)
	find_package(Qt5Widgets REQUIRED)
	set(SOURCES ${SOURCES}
		gui/gui.cpp
		gui/mainwindow.cpp
		gui/server.cpp
		gui/localserver.cpp
		gui/sidebarmodel.cpp
		gui/sidebaritemdelegate.cpp
		gui/serversummarypage.cpp
		gui/sessionpage.cpp
		gui/subheaderwidget.cpp
		gui/userlistmodel.cpp
		)
	add_definitions(-DHAVE_SERVERGUI)
endif(SERVERGUI)

# Unix specific features
if ( UNIX )
	set ( SOURCES ${SOURCES} headless/unixsignals.cpp )
endif ( UNIX )

# Select init system integration backend
if ( INITSYS STREQUAL "systemd" AND SYSTEMD_FOUND )
	set ( SOURCES ${SOURCES} initsys_systemd.cpp )
	set ( INITSYS_LIB ${SYSTEMD_LIBRARIES} )
else ()
	set ( SOURCES ${SOURCES} initsys_dummy.cpp )
endif ()

# Enable HTTP admin API if libmicrohttpd was found
if( MHD_FOUND )
	set( SOURCES ${SOURCES}
		webadmin/qmhttp.cpp
		webadmin/webadmin.cpp
	)
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_WEBADMIN" )
else( MHD_FOUND )
	message(WARNING "Libmicrohttpd not found: web-admin not enabled" )
endif( MHD_FOUND )


add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} Qt5::Network Qt5::Sql ${INITSYS_LIB} ${MHD_LIBRARIES} )
if(SERVERGUI)
	target_link_libraries( ${SRVNAME}  Qt5::Widgets )
endif()

if ( UNIX AND NOT APPLE )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ()

