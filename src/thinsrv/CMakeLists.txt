find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Sql)
find_package(libmicrohttpd QUIET)

set(srvname "drawpile-srv")
set(srvlib "${srvname}lib")

add_executable(${srvname} main.cpp)

add_library(${srvlib} STATIC
	database.cpp
	database.h
	dblog.cpp
	dblog.h
	initsys.h
	main.cpp
	multiserver.cpp
	multiserver.h
	templatefiles.cpp
	templatefiles.h
	headless/configfile.cpp
	headless/configfile.h
	headless/headless.cpp
	headless/headless.h
	$<$<BOOL:${UNIX}>:headless/unixsignals.cpp>
	$<$<BOOL:${UNIX}>:headless/unixsignals.h>
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
		gui/res/icon.svg
		gui/res/resources.qrc
		gui/accountdialog.ui
		gui/accountlistmodel.cpp
		gui/accountlistmodel.h
		gui/accountlistpage.cpp
		gui/accountlistpage.h
		gui/authdialog.cpp
		gui/authdialog.h
		gui/authdialog.ui
		gui/banlistmodel.cpp
		gui/banlistmodel.h
		gui/banlistpage.cpp
		gui/banlistpage.h
		gui/gui.cpp
		gui/gui.h
		gui/ipbandialog.ui
		gui/jsonlistmodel.cpp
		gui/jsonlistmodel.h
		gui/localserver.cpp
		gui/localserver.h
		gui/mainwindow.cpp
		gui/mainwindow.h
		gui/pagefactory.h
		gui/remoteserver.cpp
		gui/remoteserver.h
		gui/server.cpp
		gui/server.h
		gui/serverlogmodel.cpp
		gui/serverlogmodel.h
		gui/serverlogpage.cpp
		gui/serverlogpage.h
		gui/serversummarypage.cpp
		gui/serversummarypage.h
		gui/sessionlistmodel.cpp
		gui/sessionlistmodel.h
		gui/sessionlistpage.cpp
		gui/sessionlistpage.h
		gui/sessionpage.cpp
		gui/sessionpage.h
		gui/settings.ui
		gui/sidebaritemdelegate.cpp
		gui/sidebaritemdelegate.h
		gui/sidebarmodel.cpp
		gui/sidebarmodel.h
		gui/singleinstance.cpp
		gui/singleinstance.h
		gui/subheaderwidget.cpp
		gui/subheaderwidget.h
		gui/trayicon.cpp
		gui/trayicon.h
		gui/userlistmodel.cpp
		gui/userlistmodel.h
		gui/userlistpage.cpp
		gui/userlistpage.h
	)

	target_compile_definitions(${srvlib} PRIVATE HAVE_SERVERGUI)
	target_link_libraries(${srvlib} ${QT_PACKAGE_NAME}::Widgets)
endif()

if(UNIX AND NOT APPLE)
	if(INITSYS STREQUAL "systemd")
		find_package(PkgConfig QUIET)
		if(PKGCONFIG_FOUND)
			pkg_check_modules(libsystemd QUIET IMPORTED_TARGET libsystemd)
		endif()
	endif()
	add_feature_info("Server init system integration (INITSYS)" libsystemd_FOUND "")
endif()

if(libsystemd_FOUND)
	target_sources(${srvlib} PRIVATE
		initsys_systemd.cpp
	)
	target_link_libraries(${srvlib} PkgConfig::libsystemd)
else()
	target_sources(${srvlib} PRIVATE
		initsys_dummy.cpp
	)
endif()

if(libmicrohttpd_FOUND)
	target_sources(${srvlib} PRIVATE
		webadmin/qmhttp.cpp
		webadmin/qmhttp.h
		webadmin/webadmin.cpp
		webadmin/webadmin.h
	)
	target_compile_definitions(${srvlib} PRIVATE HAVE_WEBADMIN)
	target_link_libraries(${srvlib} libmicrohttpd::libmicrohttpd)
endif()
add_feature_info("Server web-admin support" libmicrohttpd_FOUND "")

if(UNIX AND NOT APPLE)
	install(TARGETS ${srvname})
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(${srvname} ${srvlib})
