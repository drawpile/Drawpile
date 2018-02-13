find_package(Qt5Widgets REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(QtColorWidgets)

include_directories("../client")

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
	scene/canvasscene.cpp
	scene/canvasview.cpp
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
	dialogs/sessionlistingdialog.cpp
	dialogs/flipbook.cpp
	dialogs/resetdialog.cpp
	dialogs/sessionsettings.cpp
	dialogs/serverlogdialog.cpp
	dialogs/tablettester.cpp
	widgets/viewstatus.cpp
	widgets/palettewidget.cpp 
	widgets/popupmessage.cpp
	widgets/netstatus.cpp 
	widgets/chatlineedit.cpp
	widgets/chatwidget.cpp 
	widgets/colorbutton.cpp
	widgets/brushpreview.cpp
	widgets/kis_curve_widget.cpp
	widgets/keysequenceedit.cpp
	widgets/groupedtoolbutton.cpp
	widgets/userlistwidget.cpp 
	widgets/filmstrip.cpp 
	widgets/resizerwidget.cpp
	widgets/tablettest.cpp
	widgets/flipbookview.cpp
	docks/utils.cpp
	docks/toolsettingsdock.cpp
	docks/brushpalettedock.cpp
	docks/navigator.cpp 
	docks/colorbox.cpp
	docks/layerlistdock.cpp
	docks/layerlistdelegate.cpp
	docks/layeraclmenu.cpp
	docks/inputsettingsdock.cpp
	utils/netfiles.cpp
	utils/mandatoryfields.cpp
	utils/recentfiles.cpp
	utils/listserverdelegate.cpp
)

IF ( APPLE )
	set ( SOURCES ${SOURCES} widgets/macmenu.cpp )
ENDIF ( APPLE )

IF(WIN32 AND WINDOWSINK)
	set ( SOURCES ${SOURCES} bundled/kis_tablet/kis_tablet_support_win8.cpp )
ENDIF()

if(GIF_FOUND)
	add_definitions(-DHAVE_GIFLIB)
endif()

if(LIBMINIUPNPC_FOUND)
	add_definitions(-DHAVE_UPNP)
endif()

if(LIBMINIUPNPC_FOUND)
	add_definitions(-DHAVE_UPNP)
endif()

if(KF5DNSSD_FOUND) 
	add_definitions(-DHAVE_DNSSD)
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
	ui/layerbox.ui
	ui/userbox.ui
	ui/newdialog.ui
	ui/resizedialog.ui
	ui/videoexport.ui
	ui/inputcfg.ui
	ui/recfilter.ui
	ui/logindialog.ui
	ui/colorbox.ui
	ui/netstats.ui
	ui/sessionlisting.ui
	ui/flipbook.ui
	ui/playback.ui
	ui/resetsession.ui
	ui/sessionsettings.ui
	ui/serverlog.ui
	ui/tablettest.ui
	ui/abusereport.ui
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
		COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/../client/*.qm" "${ResourcesDir}/i18n"
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

target_link_libraries(${CLIENTNAME} ${DPCLIENTLIB} Qt5::Widgets Qt5::Multimedia Qt5::Svg)

if(QTCOLORWIDGETS_FOUND)
	target_link_libraries(${CLIENTNAME} ${LIBQTCOLORWIDGETS_LIBRARIES})
endif()

set_target_properties (
	${CLIENTNAME}
	PROPERTIES
        VERSION ${DRAWPILE_VERSION}
	PROJECT_LABEL drawpile-client
)

install ( TARGETS ${CLIENTNAME} DESTINATION bin )

if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
	strip_exe ( ${CLIENTNAME} )
endif ( )

