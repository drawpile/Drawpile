find_package(Qt5Widgets REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(QtColorWidgets)
find_package(Vpx)
find_package(Qt5Keychain)

include_directories("../libclient")

set (
	SOURCES
	main.cpp
	mainwindow.cpp
	notifications.cpp
	toolwidgets/toolsettings.cpp
	toolwidgets/brushsettings.cpp
	toolwidgets/colorpickersettings.cpp
	toolwidgets/selectionsettings.cpp
	toolwidgets/annotationsettings.cpp
	toolwidgets/fillsettings.cpp
	toolwidgets/lasersettings.cpp
	toolwidgets/zoomsettings.cpp
	toolwidgets/inspectorsettings.cpp
	scene/canvasscene.cpp
	scene/canvasview.cpp
	scene/canvasviewmodifiers.cpp
	scene/canvasitem.cpp
	scene/annotationitem.cpp
	scene/selectionitem.cpp
	scene/usermarkeritem.cpp
	scene/lasertrailitem.cpp
	dialogs/abusereport.cpp
	dialogs/certificateview.cpp
	dialogs/newdialog.cpp 
	dialogs/hostdialog.cpp
	dialogs/joindialog.cpp
	dialogs/logindialog.cpp
	dialogs/settingsdialog.cpp
	dialogs/resizedialog.cpp
	dialogs/playbackdialog.cpp
	dialogs/videoexportdialog.cpp
	dialogs/recfilterdialog.cpp
	dialogs/netstats.cpp
	dialogs/flipbook.cpp
	dialogs/resetdialog.cpp
	dialogs/sessionsettings.cpp
	dialogs/serverlogdialog.cpp
	dialogs/tablettester.cpp
	dialogs/avatarimport.cpp
	dialogs/versioncheckdialog.cpp
	dialogs/addserverdialog.cpp
	dialogs/inputsettings.cpp
	widgets/viewstatus.cpp
	widgets/palettewidget.cpp 
	widgets/popupmessage.cpp
	widgets/netstatus.cpp 
	widgets/colorbutton.cpp
	widgets/brushpreview.cpp
	widgets/kis_curve_widget.cpp
	widgets/keysequenceedit.cpp
	widgets/groupedtoolbutton.cpp
	widgets/filmstrip.cpp 
	widgets/resizerwidget.cpp
	widgets/tablettest.cpp
	widgets/flipbookview.cpp
	widgets/presetselector.cpp
	widgets/spinner.cpp
	widgets/modifierkeys.cpp
	widgets/notifbar.cpp
	docks/utils.cpp
	docks/toolsettingsdock.cpp
	docks/brushpalettedock.cpp
	docks/navigator.cpp 
	docks/colorbox.cpp
	docks/layerlistdock.cpp
	docks/layerlistdelegate.cpp
	docks/layeraclmenu.cpp
	chat/chatlineedit.cpp
	chat/chatwidgetpinnedarea.cpp
	chat/chatwidget.cpp 
	chat/useritemdelegate.cpp
	chat/chatbox.cpp
	chat/chatwindow.cpp
	utils/mandatoryfields.cpp
	utils/recentfiles.cpp
	utils/listserverdelegate.cpp
	utils/hotbordereventfilter.cpp
)

IF ( APPLE )
	set ( SOURCES ${SOURCES} widgets/macmenu.cpp )
ENDIF ( APPLE )

IF(KIS_TABLET)
	if(WIN32)
		set (SOURCES ${SOURCES}
			bundled/kis_tablet/kis_tablet_support_win8.cpp
			bundled/kis_tablet/kis_tablet_support_win.cpp
		)
	elseif (NOT APPLE)
		set (SOURCES ${SOURCES}
			bundled/kis_tablet/kis_xi2_event_filter.cpp
			bundled/kis_tablet/qxcbconnection.cpp
			bundled/kis_tablet/qxcbconnection_xi2.cpp
		)

		find_package(X11 REQUIRED COMPONENTS Xinput)
		find_package(XCB REQUIRED COMPONENTS XCB)
		find_package(Qt5X11Extras REQUIRED)

		set (EXTRALIBS Qt5::X11Extras ${X11_Xinput_LIB} ${XCB_LIBRARIES} ${X11_LIBRARIES} )
	endif()
ENDIF(KIS_TABLET)

if(GIF_FOUND)
	add_definitions(-DHAVE_GIFLIB)
endif()

if(LIBMINIUPNPC_FOUND)
	add_definitions(-DHAVE_UPNP)
endif()

if(KF5DNSSD_FOUND)
	add_definitions(-DHAVE_DNSSD)
	# KF5DNSSD 8.84.0 moves a bunch of headers to a different place.
	if("${KF5DNSSD_VERSION}" VERSION_LESS "5.84.0")
		add_definitions(-DHAVE_DNSSD_BEFORE_5_84_0)
	endif()
endif()

if(LIBVPX_FOUND)
	add_definitions(-DHAVE_WEBM)
endif()

if(QTKEYCHAIN_LIBRARIES)
	add_definitions(-DHAVE_QTKEYCHAIN)
else ()
	message(WARNING "QtKeyChain not found: password remembering not enabled")
endif()

set (
	UIs
	ui/brushdock.ui
	ui/brushpalette.ui
	ui/certificateview.ui
	ui/hostdialog.ui
	ui/joindialog.ui
	ui/settings.ui
	ui/textsettings.ui
	ui/selectsettings.ui
	ui/fillsettings.ui
	ui/lasersettings.ui
	ui/inspectorsettings.ui
	ui/layerbox.ui
	ui/newdialog.ui
	ui/resizedialog.ui
	ui/videoexport.ui
	ui/inputcfg.ui
	ui/recfilter.ui
	ui/logindialog.ui
	ui/colorbox.ui
	ui/netstats.ui
	ui/flipbook.ui
	ui/playback.ui
	ui/resetsession.ui
	ui/sessionsettings.ui
	ui/serverlog.ui
	ui/tablettest.ui
	ui/abusereport.ui
	ui/avatarimport.ui
	ui/navigator.ui
	ui/versioncheck.ui
)

# Use bundled QtColorWidgets widgets if library not found
if(QTCOLORWIDGETS_FOUND)
	include_directories(${LIBQTCOLORWIDGETS_INCLUDE_DIRS})
else()
	message(WARNING "QtColorWidgets not found: using bundled copy!")
	set(
		SOURCES ${SOURCES}
		bundled/QtColorWidgets/color_wheel.cpp
		bundled/QtColorWidgets/color_preview.cpp
		bundled/QtColorWidgets/color_dialog.cpp
		bundled/QtColorWidgets/gradient_slider.cpp
		bundled/QtColorWidgets/hue_slider.cpp
		bundled/QtColorWidgets/color_line_edit.cpp
		bundled/QtColorWidgets/color_utils.cpp
		bundled/QtColorWidgets/color_names.cpp
	)
	qt5_add_resources( QtResource bundled/QtColorWidgets/color_widgets.qrc )
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/bundled/QtColorWidgets")
	add_definitions(-DQTCOLORPICKER_STATICALLY_LINKED)
endif()

qt5_wrap_ui( UI_Headers ${UIs} )

qt5_add_resources( QtResource ui/cursors/cursors.qrc )
qt5_add_resources( QtResource ui/icons/builtin.qrc )

# non-qt stuff

set ( EXETYPE "" )
set ( RES "" )
if ( WIN32 )
	if ( NOT CMAKE_BUILD_TYPE STREQUAL Debug )
		set ( EXETYPE WIN32 )
	endif ( )
	
	generate_win32_resource (
		Win32Resource
		"Drawpile client" # full name
		${CLIENTNAME} # internal name
		"Drawpile client"
		${WEBSITE}
		"2006-2017 Calle Laakkonen"
		${DRAWPILE_VERSION}
		"${CMAKE_SOURCE_DIR}/desktop/drawpile.ico"
	)
endif ( WIN32 )

IF ( APPLE )
	SET(MACOSX_BUNDLE_BUNDLE_NAME ${CLIENTNAME})
	# set how it shows up in the Info.plist file
	SET(MACOSX_BUNDLE_ICON_FILE "drawpile.icns")
	# set the bundle identifier (REQUIRED, or some strange GUI bugs may appear)
	SET(MACOSX_BUNDLE_GUI_IDENTIFIER "net.drawpile.DrawpileClient")
	#SET(MACOSX_BUNDLE_INFO_STRING "${CLIENTNAME}")
	#SET(MACOSX_BUNDLE_LONG_VERSION_STRING "${CLIENTNAME}")
	SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "${DRAWPILE_VERSION}")
	SET(MACOSX_BUNDLE_BUNDLE_VERSION "${DRAWPILE_VERSION}")
	SET(MACOSX_BUNDLE_COPYRIGHT "Copyright Drawpile team")

	SET(ResourcesDir "${CMAKE_BINARY_DIR}/bin/${CLIENTNAME}.app/Contents/Resources")
	add_custom_target( appbundle ALL
		COMMAND mkdir -p "${ResourcesDir}/i18n"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/drawpile.icns" "${ResourcesDir}"
		COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/../libclient/*.qm" "${ResourcesDir}/i18n"
		COMMAND mkdir -p "${ResourcesDir}/palettes"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/palettes/*.gpl" "${ResourcesDir}/palettes"
		COMMAND mkdir -p "${ResourcesDir}/sounds"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/sounds/*.wav" "${ResourcesDir}/sounds"
		COMMAND cp -r "${CMAKE_SOURCE_DIR}/desktop/theme" "${ResourcesDir}"
	)
ENDIF()

add_executable (
	${CLIENTNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${SOURCES}
	${QtResource}
	${QM_TRANSLATIONS}
	${Win32Resource}
	${UI_Headers} # required here for ui_*.h generation
)

target_link_libraries(${CLIENTNAME}
	dpclient
	thicksrvlib
	Qt5::Widgets
	Qt5::Multimedia
	Qt5::Svg
	${EXTRALIBS}
)

if(QTCOLORWIDGETS_FOUND)
	target_link_libraries(${CLIENTNAME} ${LIBQTCOLORWIDGETS_LIBRARIES})
endif()

if(QTKEYCHAIN_LIBRARIES)
	target_link_libraries(${CLIENTNAME} ${QTKEYCHAIN_LIBRARIES})
endif()

set_target_properties (
	${CLIENTNAME}
	PROPERTIES
        VERSION ${DRAWPILE_VERSION}
	PROJECT_LABEL drawpile-client
)

install ( TARGETS ${CLIENTNAME} DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS} )

if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
	strip_exe ( ${CLIENTNAME} )
endif ( )

