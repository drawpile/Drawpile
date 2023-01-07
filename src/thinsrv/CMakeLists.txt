find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Sql)
find_package(Libmicrohttpd QUIET)
find_package(Sodium QUIET)

set(srvname "drawpile-srv")
set(srvlib "${srvname}lib")

add_executable(${srvname} main.cpp)

add_library(${srvlib} STATIC
	multiserver.cpp
	database.cpp
	dblog.cpp
	templatefiles.cpp
	headless/headless.cpp
	headless/configfile.cpp
	$<$<BOOL:${UNIX}>:headless/unixsignals.cpp>
)

target_link_libraries(${srvlib}
	dpserver
	${QT_PACKAGE_NAME}::Network
	${QT_PACKAGE_NAME}::Sql
)

target_link_libraries(${srvname} ${srvlib})

if(SERVERGUI)
	find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Widgets)

	target_sources(${srvlib} PRIVATE
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
		gui/res/resources.qrc
	)

	target_compile_definitions(${srvlib} PRIVATE HAVE_SERVERGUI)
	target_link_libraries(${srvlib} ${QT_PACKAGE_NAME}::Widgets)
endif()

string(TOLOWER "${INITSYS}" INITSYS)
if(INITSYS STREQUAL "systemd")
	find_package(PkgConfig QUIET)
	if(PKGCONFIG_FOUND)
		pkg_check_modules(SYSTEMD "libsystemd" QUIET)
	endif()
endif()

if(SYSTEMD_FOUND)
	target_sources(${srvlib} PRIVATE
		initsys_systemd.cpp
	)
	target_link_libraries(${srvlib} ${SYSTEMD_LIBRARIES})
else()
	target_sources(${srvlib} PRIVATE
		initsys_dummy.cpp
	)
endif()
add_feature_info("Server systemd integration" SYSTEMD_FOUND "")

if(MHD_FOUND)
	target_sources(${srvlib} PRIVATE
		webadmin/qmhttp.cpp
		webadmin/webadmin.cpp
	)
	target_compile_definitions(${srvlib} PRIVATE HAVE_WEBADMIN)
	target_link_libraries(${srvlib} ${MHD_LIBRARIES})
endif()
add_feature_info("Server web-admin support" MHD_FOUND "")

if(Sodium_FOUND)
	target_compile_definitions(${srvlib} PRIVATE HAVE_LIBSODIUM)
endif()

if(UNIX AND NOT APPLE)
	install(TARGETS ${srvname})
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(${srvname} ${srvlib})
