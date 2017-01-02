find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5LinguistTools)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(KF5DNSSD NO_MODULE)
find_package(QtColorWidgets)
find_package(GIF)
find_package(Miniupnpc)

set (
	SOURCES
	main.cpp
	mainwindow.cpp
	document.cpp
	notifications.cpp
	tools/toolcontroller.cpp
	tools/toolproperties.cpp
	tools/utils.cpp
	tools/annotation.cpp
	tools/brushes.cpp
	tools/colorpicker.cpp
	tools/laser.cpp
	tools/selection.cpp
	tools/shapetools.cpp
	tools/floodfill.cpp
	toolwidgets/toolsettings.cpp
	toolwidgets/brushsettings.cpp
	toolwidgets/shapetoolsettings.cpp
	toolwidgets/colorpickersettings.cpp
	toolwidgets/selectionsettings.cpp
	toolwidgets/annotationsettings.cpp
	toolwidgets/fillsettings.cpp
	toolwidgets/lasersettings.cpp
	canvas/statetracker.cpp
	canvas/canvasmodel.cpp
	canvas/selection.cpp
	canvas/usercursormodel.cpp
	canvas/lasertrailmodel.cpp
	canvas/retcon.cpp
	canvas/loader.cpp
	canvas/textloader.cpp
	canvas/aclfilter.cpp
	canvas/userlist.cpp
	canvas/layerlist.cpp
	canvas/history.cpp
	scene/canvasscene.cpp
	scene/canvasview.cpp
	scene/canvasitem.cpp
	scene/annotationitem.cpp
	scene/selectionitem.cpp
	scene/usermarkeritem.cpp
	scene/lasertrailitem.cpp
	net/client.cpp
	net/loopbackserver.cpp
	net/tcpserver.cpp
	net/login.cpp
	net/loginsessions.cpp
	net/serverthread.cpp
	net/builtinserver.cpp
	net/sessionlistingmodel.cpp
	net/banlistmodel.cpp
	net/commands.cpp
	utils/archive.cpp
	utils/palette.cpp
	utils/palettelistmodel.cpp
	utils/mandatoryfields.cpp
	utils/recentfiles.cpp
	utils/whatismyip.cpp
	utils/strokesmoother.cpp
	utils/html.cpp
	utils/usernamevalidator.cpp
	utils/sessionidvalidator.cpp
	utils/images.cpp
	utils/networkaccess.cpp
	utils/funstuff.cpp
	utils/kis_cubic_curve.cpp
	utils/shortcutdetector.cpp
	utils/customshortcutmodel.cpp
	utils/listservermodel.cpp
	utils/listserverdelegate.cpp
	utils/settings.cpp
	utils/icon.cpp
	utils/logging.cpp
	core/annotationmodel.cpp
	core/tile.cpp
	core/layer.cpp
	core/layerstack.cpp
	core/brush.cpp
	core/brushmask.cpp
	core/blendmodes.cpp
	core/rasterop.cpp
	core/shapes.cpp
	core/floodfill.cpp
	ora/orawriter.cpp
	ora/orareader.cpp
	recording/index.cpp
	recording/indexbuilder.cpp
	recording/indexloader.cpp
	recording/filter.cpp
	recording/playbackcontroller.cpp
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
	dialogs/banlistdialog.cpp
	widgets/viewstatus.cpp
	widgets/palettewidget.cpp 
	widgets/popupmessage.cpp
	widgets/netstatus.cpp 
	widgets/chatlineedit.cpp
	widgets/chatwidget.cpp 
	widgets/colorbutton.cpp
	widgets/brushpreview.cpp
	widgets/toolslotbutton.cpp
	widgets/kis_curve_widget.cpp
	widgets/keysequenceedit.cpp
	widgets/groupedtoolbutton.cpp
	widgets/userlistwidget.cpp 
	widgets/filmstrip.cpp 
	docks/utils.cpp
	docks/toolsettingsdock.cpp 
	docks/navigator.cpp 
	docks/colorbox.cpp
	docks/layerlistdock.cpp
	docks/layerlistdelegate.cpp
	docks/layeraclmenu.cpp
	docks/inputsettingsdock.cpp
	export/animation.cpp
	export/videoexporter.cpp
	export/imageseriesexporter.cpp
	export/ffmpegexporter.cpp
)

IF ( APPLE )
	set ( SOURCES ${SOURCES} widgets/macmenu.cpp )
ENDIF ( APPLE )

if(GIF_FOUND)
	set ( SOURCES ${SOURCES} export/gifexporter.cpp )
	add_definitions(-DHAVE_GIFLIB)
	include_directories(SYSTEM "${GIF_INCLUDE_DIR}")
endif()

if(LIBMINIUPNPC_FOUND)
	set ( SOURCES ${SOURCES} net/upnp.cpp )
	add_definitions(-DHAVE_UPNP)
endif()

set (
	UIs
	ui/brushsettings.ui
	ui/simplesettings.ui
	ui/smudgesettings.ui
	ui/certificateview.ui
	ui/hostdialog.ui
	ui/joindialog.ui
	ui/settings.ui
	ui/textsettings.ui
	ui/pensettings.ui
	ui/erasersettings.ui
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
)

# Optional DNS-SD support
if(KF5DNSSD_FOUND) 
	set(
		SOURCES ${SOURCES}
		net/serverdiscoverymodel.cpp
	)
endif()

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

if( Qt5LinguistTools_FOUND)
	set(TRANSLATIONS
		i18n/drawpile_fi.ts
		i18n/drawpile_cs.ts
		i18n/drawpile_ja.ts
		i18n/drawpile_ru.ts
		i18n/drawpile_de.ts
		i18n/drawpile_zh.ts
	)

	qt5_add_translation(QM_TRANSLATIONS ${TRANSLATIONS})

endif()


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
		"2006-2015 Calle Laakkonen"
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
		COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/*.qm" "${ResourcesDir}/i18n"
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

target_link_libraries(${CLIENTNAME} ${DPSHAREDLIB} Qt5::Widgets Qt5::Network Qt5::Xml Qt5::Concurrent Qt5::Multimedia Qt5::Svg)

if(KF5DNSSD_FOUND) 
    add_definitions(-DHAVE_DNSSD) 
	target_link_libraries(${CLIENTNAME} KF5::DNSSD)
endif() 

if(QTCOLORWIDGETS_FOUND)
	target_link_libraries(${CLIENTNAME} ${LIBQTCOLORWIDGETS_LIBRARIES})
endif()

if(GIF_FOUND)
	target_link_libraries(${CLIENTNAME} ${GIF_LIBRARIES})
endif()

if(LIBMINIUPNPC_FOUND)
	target_link_libraries(${CLIENTNAME} ${LIBMINIUPNPC_LIBRARIES})
        if ( WIN32 )
                target_link_libraries (${CLIENTNAME} iphlpapi)
        endif ( WIN32 )

endif()

if ( WIN32 )
        target_link_libraries (${CLIENTNAME} ws2_32)
endif ()

set_target_properties (
	${CLIENTNAME}
	PROPERTIES
        VERSION ${DRAWPILE_VERSION}
	PROJECT_LABEL drawpile-client
)

install ( TARGETS ${CLIENTNAME} DESTINATION bin )

if( QM_TRANSLATIONS )
	install( FILES ${QM_TRANSLATIONS} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile/i18n" )
endif( QM_TRANSLATIONS)

if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
	if (APPLE)
		strip_exe ( ${CLIENTNAME}.app/Contents/MacOS/${CLIENTNAME} )
	else ()
		strip_exe ( ${CLIENTNAME} )
	endif()
endif ( )

