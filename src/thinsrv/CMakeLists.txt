set(thinsrv_sources
	database.cpp
	dblog.cpp
	headless/configfile.cpp
	headless/headless.cpp
	multiserver.cpp
	templatefiles.cpp
)

set(thinsrv_gui_sources
	gui/accountlistmodel.cpp
	gui/accountlistpage.cpp
	gui/authdialog.cpp
	gui/banlistmodel.cpp
	gui/banlistpage.cpp
	gui/gui.cpp
	gui/jsonlistmodel.cpp
	gui/localserver.cpp
	gui/mainwindow.cpp
	gui/remoteserver.cpp
	gui/server.cpp
	gui/serverlogmodel.cpp
	gui/serverlogpage.cpp
	gui/serversummarypage.cpp
	gui/sessionlistmodel.cpp
	gui/sessionlistpage.cpp
	gui/sessionpage.cpp
	gui/sidebaritemdelegate.cpp
	gui/sidebarmodel.cpp
	gui/singleinstance.cpp
	gui/subheaderwidget.cpp
	gui/trayicon.cpp
	gui/userlistmodel.cpp
	gui/userlistpage.cpp
)

set(thinsrv_gui_uis
	gui/accountdialog.ui
	gui/authdialog.ui
	gui/ipbandialog.ui
	gui/settings.ui
)

add_library("${SRVNAME}lib" STATIC ${thinsrv_sources})
set_drawpile_target_properties(dpshared)
target_include_directories("${SRVNAME}lib" PUBLIC
	"${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries("${SRVNAME}lib" PUBLIC dpserver DP::QtNetwork DP::QtSql)

if(SERVERGUI)
	dp_qt_wrap_ui(thinsrv_ui_headers ${thinsrv_gui_uis})
	dp_qt_add_resources(thinsrv_qt_resources gui/res/resources.qrc)
	message(STATUS "thinsrv_ui_headers ${thinsrv_ui_headers}")
	target_sources("${SRVNAME}lib" PRIVATE
		${thinsrv_gui_sources}
		${thinsrv_ui_headers})
	target_compile_definitions("${SRVNAME}lib" PUBLIC HAVE_SERVERGUI)
	target_link_libraries("${SRVNAME}lib" PUBLIC DP::QtWidgets)
endif()

if(UNIX)
	target_sources("${SRVNAME}lib" PRIVATE headless/unixsignals.cpp)
endif()

# Select init system integration backend
if(INITSYS STREQUAL "systemd" AND SYSTEMD_FOUND)
	target_sources("${SRVNAME}lib" PRIVATE initsys_systemd.cpp)
	target_link_libraries("${SRVNAME}lib" PUBLIC ${SYSTEMD_LIBRARIES})
else()
	target_sources("${SRVNAME}lib" PRIVATE initsys_dummy.cpp)
endif()

# Enable HTTP admin API if libmicrohttpd was found
if(MHD_FOUND)
	target_sources("${SRVNAME}lib" PRIVATE webadmin/qmhttp.cpp webadmin/webadmin.cpp)
    target_compile_definitions("${SRVNAME}lib" PUBLIC HAVE_WEBADMIN)
	target_link_libraries("${SRVNAME}lib" PUBLIC ${MHD_LIBRARIES})
else()
	message(WARNING "Libmicrohttpd not found: web-admin not enabled" )
endif()

# Enable ext-auth if libsodium is available
if(Sodium_FOUND)
    target_compile_definitions("${SRVNAME}lib" PUBLIC HAVE_LIBSODIUM)
endif()

add_executable(${SRVNAME} main.cpp ${thinsrv_qt_resources})
set_drawpile_target_properties(${SRVNAME})
target_link_libraries(${SRVNAME} PUBLIC "${SRVNAME}lib")

if(UNIX AND NOT APPLE)
	install(TARGETS ${SRVNAME} DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

