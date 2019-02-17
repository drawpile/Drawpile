# src/server/CMakeLists.txt

find_package( Qt5Sql REQUIRED)
find_package( Libmicrohttpd )
find_package(Sodium)

set (
	SOURCES
	multiserver.cpp
	sslserver.cpp
	database.cpp
	dblog.cpp
	templatefiles.cpp
	headless/headless.cpp
	headless/configfile.cpp
	)

if(SERVERGUI)
	find_package(Qt5Widgets REQUIRED)

	qt5_wrap_ui( UI_Headers gui/ipbandialog.ui gui/accountdialog.ui gui/settings.ui gui/authdialog.ui )
	qt5_add_resources( QtResource gui/res/resources.qrc )

	set(SOURCES ${SOURCES} ${UI_Headers}
		gui/gui.cpp
		gui/singleinstance.cpp
		gui/trayicon.cpp
		gui/authdialog.cpp
		gui/mainwindow.cpp
		gui/server.cpp
		gui/localserver.cpp
		gui/remoteserver.cpp
		gui/sidebarmodel.cpp
		gui/sidebaritemdelegate.cpp
		gui/serversummarypage.cpp
		gui/sessionlistpage.cpp
		gui/userlistpage.cpp
		gui/banlistpage.cpp
		gui/accountlistpage.cpp
		gui/sessionpage.cpp
		gui/subheaderwidget.cpp
		gui/jsonlistmodel.cpp
		gui/userlistmodel.cpp
		gui/sessionlistmodel.cpp
		gui/banlistmodel.cpp
		gui/accountlistmodel.cpp
		gui/serverlogmodel.cpp
		gui/serverlogpage.cpp
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

# Enable ext-auth if libsodium is available
if( Sodium_FOUND )
    add_definitions(-DHAVE_LIBSODIUM)
endif( Sodium_FOUND )


add_library( "${SRVNAME}lib" STATIC ${SOURCES} )
target_link_libraries( "${SRVNAME}lib"  ${DPSHAREDLIB} Qt5::Network Qt5::Sql ${INITSYS_LIB} ${MHD_LIBRARIES} )
if(SERVERGUI)
	target_link_libraries( "${SRVNAME}lib"  Qt5::Widgets )
endif()

add_executable(${SRVNAME} main.cpp ${QtResource})
target_link_libraries(${SRVNAME} "${SRVNAME}lib")

if ( UNIX AND NOT APPLE )
	install ( TARGETS ${SRVNAME} DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS} )
endif ()

if(TESTS)
	add_subdirectory(tests)
endif(TESTS)

