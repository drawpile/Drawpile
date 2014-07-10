find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5LinguistTools)

find_package(KF5Archive NO_MODULE)
find_package(QtColorPicker)

set (
	SOURCES
	main.cpp
	mainwindow.cpp
	statetracker.cpp
	loader.cpp
	textloader.cpp
	tools/toolsettings.cpp
	tools/toolproperties.cpp
	tools/tool.cpp
	tools/utils.cpp
	tools/annotation.cpp
	tools/brushes.cpp
	tools/colorpicker.cpp
	tools/laser.cpp
	tools/selection.cpp
	tools/shapetools.cpp
	scene/canvasscene.cpp
	scene/canvasview.cpp
	scene/canvasitem.cpp
	scene/annotationitem.cpp
	scene/selectionitem.cpp
	scene/usermarkeritem.cpp
	scene/lasertrailitem.cpp
	scene/strokepreviewer.cpp
	net/client.cpp
	net/loopbackserver.cpp
	net/tcpserver.cpp
	net/utils.cpp
	net/login.cpp
	net/loginsessions.cpp
	net/userlist.cpp
	net/layerlist.cpp
	net/serverthread.cpp
	net/builtinserver.cpp
	utils/archive.cpp
	utils/palette.cpp
	utils/mandatoryfields.cpp
	utils/recentfiles.cpp
	utils/whatismyip.cpp
	utils/strokesmoother.cpp
	utils/html.cpp
	utils/usernamevalidator.cpp
	utils/sessionidvalidator.cpp
	utils/imagesize.cpp
	core/annotation.cpp
	core/tile.cpp
	core/layer.cpp
	core/layerstack.cpp
	core/brush.cpp
	core/brushmask.cpp
	core/rasterop.cpp
	core/shapes.cpp
	ora/orawriter.cpp
	ora/orareader.cpp
	recording/index.cpp
	recording/indexbuilder.cpp
	recording/indexloader.cpp
	recording/filter.cpp
	recording/indexgraphicsitem.cpp
	recording/indexpointergraphicsitem.cpp
	recording/indexgraphicsview.cpp
	dialogs/certificateview.cpp
	dialogs/newdialog.cpp 
	dialogs/hostdialog.cpp
	dialogs/joindialog.cpp
	dialogs/logindialog.cpp
	dialogs/settingsdialog.cpp
	dialogs/resizedialog.cpp
	dialogs/playbackdialog.cpp
	dialogs/tinyplayer.cpp
	dialogs/videoexportdialog.cpp
	dialogs/recfilterdialog.cpp
	dialogs/selectsessiondialog.cpp
	widgets/viewstatus.cpp
	widgets/palettewidget.cpp 
	widgets/popupmessage.cpp
	widgets/netstatus.cpp 
	widgets/dualcolorbutton.cpp
	widgets/chatlineedit.cpp
	widgets/chatwidget.cpp 
	widgets/colorbutton.cpp 
	widgets/imageselector.cpp
	widgets/brushpreview.cpp
	widgets/toolslotbutton.cpp
	docks/toolsettingsdock.cpp 
	docks/palettebox.cpp 
	docks/navigator.cpp 
	docks/colorbox.cpp
	docks/userlistdock.cpp 
	docks/layerlistdock.cpp
	docks/layerlistdelegate.cpp
	docks/layeraclmenu.cpp
	docks/inputsettingsdock.cpp
	export/videoexporter.cpp
	export/imageseriesexporter.cpp
	export/ffmpegexporter.cpp
	bundled/kis_curve_widget.cpp
	bundled/kis_cubic_curve.cpp
)

set (
	UIs
	ui/brushsettings.ui
	ui/simplesettings.ui
	ui/certificateview.ui
	ui/hostdialog.ui
	ui/joindialog.ui
	ui/palettebox.ui
	ui/settings.ui
	ui/navibox.ui
	ui/textsettings.ui
	ui/pensettings.ui
	ui/erasersettings.ui
	ui/selectsettings.ui
	ui/lasersettings.ui
	ui/layerbox.ui
	ui/userbox.ui
	ui/newdialog.ui
	ui/resizedialog.ui
	ui/playback.ui
	ui/videoexport.ui
	ui/inputcfg.ui
	ui/recfilter.ui
	ui/tinyplayer.ui
	ui/selectsession.ui
	ui/logindialog.ui
)

# Use bundled KArchive if library not found
if(NOT KF5Archive_FOUND)
	message(WARNING "KArchive not found: using bundled copy!")

	find_package (ZLIB REQUIRED)

	set(
		SOURCES ${SOURCES}
		bundled/karchive/karchive.cpp
		bundled/karchive/klimitediodevice.cpp
		bundled/karchive/kzip.cpp
		bundled/karchive/kcompressiondevice.cpp
		bundled/karchive/kfilterbase.cpp
		bundled/karchive/kfilterdev.cpp
		bundled/karchive/klimitediodevice.cpp
		bundled/karchive/kgzipfilter.cpp
		bundled/karchive/knonefilter.cpp
	)
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/bundled/karchive")

endif()

# Use bundled QtColorPicker widgets if library not found
if(QTCOLORPICKER_FOUND)
	include_directories(${LIBQTCOLORPICKER_INCLUDE_DIRS})
else()
	message(WARNING "QtColorPicker not found: using bundled copy!")
	set(
		SOURCES ${SOURCES}
		bundled/QtColorPicker/color_wheel.cpp
		bundled/QtColorPicker/color_preview.cpp
		bundled/QtColorPicker/color_dialog.cpp
		bundled/QtColorPicker/gradient_slider.cpp
		bundled/QtColorPicker/hue_slider.cpp
	)
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/bundled/QtColorPicker")
endif()

qt5_wrap_ui( UI_Headers ${UIs} )

if( BUILTIN_ICONS )
	message(STATUS "Including icons in the executable" )
	qt5_add_resources( QtResource ui/icons/icons.qrc )
endif( BUILTIN_ICONS )

if( Qt5LinguistTools_FOUND)
	set(TRANSLATIONS
		i18n/drawpile_fi.ts
	)

	qt5_add_translation(QM_TRANSLATIONS ${TRANSLATIONS})

	INSTALL(FILES "${CMAKE_SOURCE_DIR}/desktop/drawpile.icns" DESTINATION ${CMAKE_INSTALL_PREFIX}/${CLIENTNAME}.app/Contents/Resources COMPONENT ${CLIENTNAME})

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
		"2006-2014 Calle Laakkonen"
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
	)
ENDIF()

add_executable (
	${CLIENTNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${MACOSX_BUNDLE_INFO_PLIST}
	${SOURCES}
	${QtResource}
	${QM_TRANSLATIONS}
	${Win32Resource}
	${UI_Headers} # required here for ui_*.h generation
)

target_link_libraries(${CLIENTNAME} ${DPSHAREDLIB} Qt5::Widgets Qt5::Network Qt5::Xml Qt5::Concurrent)

# Shouldn't be needed anymore, but ubuntu 13.10 cmake doesn't work without this:
qt5_use_modules(${CLIENTNAME} Widgets Network Xml Concurrent)

if(KF5Archive_FOUND)
	target_link_libraries(${CLIENTNAME} KF5::Archive)
else()
	target_link_libraries(${CLIENTNAME} ${ZLIB_LIBRARIES})
endif()

if(QTCOLORPICKER_FOUND)
	target_link_libraries(${CLIENTNAME} ${LIBQTCOLORPICKER_LIBRARIES})
endif()

if ( WIN32 )
		target_link_libraries (${CLIENTNAME} ws2_32 )
endif ()

set_target_properties (
	${CLIENTNAME}
	PROPERTIES
        VERSION ${DRAWPILE_VERSION}
	PROJECT_LABEL drawpile-client
)

install ( TARGETS ${CLIENTNAME} DESTINATION bin )

if( NOT BUILTIN_ICONS )
	install( DIRECTORY ui/icons DESTINATION "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile" )
endif()

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

