find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Sql)
find_package(libmicrohttpd QUIET)

set(srvname "drawpile-srv")
set(srvlib "${srvname}lib")

add_executable(${srvname} main.cpp)

add_library(${srvlib})

target_sources(${srvlib} PRIVATE
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
)

# https://gitlab.kitware.com/cmake/cmake/-/issues/19813
if(UNIX)
	target_sources(${srvlib} PRIVATE
		headless/unixsignals.cpp
		headless/unixsignals.h
	)
endif()

target_link_libraries(${srvlib} PUBLIC
	dpshared
	dpserver
	${QT_PACKAGE_NAME}::Core
	${QT_PACKAGE_NAME}::Network
	${QT_PACKAGE_NAME}::Sql
)

target_link_libraries(${srvname} PRIVATE ${srvlib})

if(SERVERGUI)
	find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Gui Widgets)

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
	target_link_libraries(${srvlib} PRIVATE
		${QT_PACKAGE_NAME}::Gui
		${QT_PACKAGE_NAME}::Widgets
	)
endif()

if(UNIX AND NOT APPLE)
	if(INITSYS STREQUAL "systemd")
		find_package(PkgConfig QUIET)
		if(PKGCONFIG_FOUND)
			pkg_check_modules(libsystemd QUIET IMPORTED_TARGET libsystemd)
		endif()
	endif()
	add_feature_info("Server init system integration (INITSYS)" "TARGET PkgConfig::libsystemd" "")
endif()

if(TARGET PkgConfig::libsystemd)
	target_sources(${srvlib} PRIVATE
		initsys_systemd.cpp
	)
	target_link_libraries(${srvlib} PRIVATE PkgConfig::libsystemd)

	if(TARGET libmicrohttpd::libmicrohttpd)
		set(webadmin_stream ListenStream=${DRAWPILE_WEBADMIN_DEFAULT_PORT})
	endif()
	configure_file(systemd/service.in ${srvname}.service @ONLY)
	configure_file(systemd/socket.in ${srvname}.socket @ONLY)
	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/${srvname}.service"
		"${CMAKE_CURRENT_BINARY_DIR}/${srvname}.socket"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system/"
		COMPONENT ${srvname}
	)
else()
	target_sources(${srvlib} PRIVATE
		initsys_dummy.cpp
	)
endif()

if(TARGET libmicrohttpd::libmicrohttpd)
	target_sources(${srvlib} PRIVATE
		webadmin/qmhttp.cpp
		webadmin/qmhttp.h
		webadmin/webadmin.cpp
		webadmin/webadmin.h
	)
	target_compile_definitions(${srvlib} PRIVATE HAVE_WEBADMIN)
	target_link_libraries(${srvlib} PRIVATE libmicrohttpd::libmicrohttpd)
endif()
add_feature_info("Server web-admin support" "TARGET libmicrohttpd::libmicrohttpd" "")

install(
	TARGETS ${srvname}
	COMPONENT ${srvname}
)

# No trailing slash on DIRECTORY is required or else it will strip the last path
# of the directory
install(
	DIRECTORY contrib
	DESTINATION ${INSTALL_APPDATADIR}
	COMPONENT ${srvname}
)

function(add_man_section name condition out_last_modified)
	if(condition)
		file(TIMESTAMP "${CMAKE_CURRENT_SOURCE_DIR}/man/thinsrv.1.${name}" last_modified_section "%Y-%m-%d" UTC)
		file(READ "man/thinsrv.1.${name}" ${name}_section)
		set(${name}_section ${${name}_section} PARENT_SCOPE)
		if(${${out_last_modified}} STRLESS ${last_modified_section})
			set(${out_last_modified} ${last_modified_section} PARENT_SCOPE)
		endif()
	endif()
endfunction()

if(INSTALL_DOC AND UNIX)
	string(TOUPPER ${srvname} srvname_upper)
	file(TIMESTAMP "${CMAKE_CURRENT_SOURCE_DIR}/man/thinsrv.1.in" last_modified "%Y-%m-%d" UTC)
	add_man_section(gui SERVERGUI last_modified)
	add_man_section(sodium libsodium_FOUND last_modified)
	add_man_section(webadmin libmicrohttpd_FOUND last_modified)
	configure_file(man/thinsrv.1.in ${srvname}.1 @ONLY)
	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/${srvname}.1"
		DESTINATION "${CMAKE_INSTALL_MANDIR}"
		COMPONENT ${srvname}
	)
	install(FILES
		"${PROJECT_SOURCE_DIR}/LICENSE.txt"
		"${PROJECT_SOURCE_DIR}/ChangeLog"
		"${PROJECT_SOURCE_DIR}/README.md"
		DESTINATION "${CMAKE_INSTALL_DOCDIR}"
		COMPONENT ${srvname}
	)
	# Trailing slash is required or else it will install into `doc` subdirectory
	install(DIRECTORY "${PROJECT_SOURCE_DIR}/doc/"
		DESTINATION "${CMAKE_INSTALL_DOCDIR}"
		COMPONENT ${srvname}
	)
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

directory_auto_source_groups()
