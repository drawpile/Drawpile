# src/client/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Widgets REQUIRED)

# Workaround the mess in ioapi.h. Mac OS X and BSDs don't need the "64" suffix for open()
# and friends.
if (APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    add_definitions(-DUSE_FILE32API) # Misleading macro name from ioapi.h
endif()

# files

set (
	HEADERS
	main.h mainwindow.h netstatus.h dualcolorbutton.h editorview.h
	toolsettingswidget.h colordialog.h brushpreview.h qtcolortriangle.h
	board.h controller.h gradientslider.h colorbutton.h
	newdialog.h imageselector.h hostdialog.h joindialog.h
	hoststate.h sessionstate.h logindialog.h
	localserver.h popupmessage.h userlistwidget.h userlistmodel.h chatwidget.h
	mandatoryfields.h palettewidget.h palettebox.h
	colorbox.h settingsdialog.h layerwidget.h
	navigator.h toolsettings.h brushslider.h viewstatus.h layerlistdelegate.h
	layerlistwidget.h core/layerstack.h core/layermimedata.h
)

# unused headers because they do not create moc_* files ()
set (
	UNUSED_HEADERS # no QOBJECT ?
	version.h user.h interfaces.h preview.h boardeditor.h boarditem.h
	palette.h icons.h
	tools.h recentfiles.h sessioninfo.h annotationitem.h
	core/tile.h core/layer.h core/brush.h core/point.h
	core/rasterop.h
	ora/zipfile.h ora/orawriter.h ora/orareader.h
	ora/zip.h ora/unzip.h ora/ioapi.h
)

set (
	SOURCES
	main.cpp mainwindow.cpp netstatus.cpp dualcolorbutton.cpp
	editorview.cpp toolsettingswidget.cpp colordialog.cpp brushpreview.cpp
	qtcolortriangle.cpp board.cpp controller.cpp boarditem.cpp user.cpp
	tools.cpp toolsettings.cpp boardeditor.cpp
	gradientslider.cpp colorbutton.cpp newdialog.cpp imageselector.cpp
	hostdialog.cpp joindialog.cpp hoststate.cpp sessionstate.cpp
	logindialog.cpp preview.cpp localserver.cpp popupmessage.cpp
	userlistwidget.cpp userlistmodel.cpp chatwidget.cpp recentfiles.cpp
	sessioninfo.cpp icons.cpp mandatoryfields.cpp
	palettewidget.cpp palettebox.cpp colorbox.cpp
	settingsdialog.cpp palette.cpp navigator.cpp annotationitem.cpp
	core/tile.cpp core/layer.cpp core/layerstack.cpp core/brush.cpp
	core/rasterop.cpp brushslider.cpp viewstatus.cpp layerlistdelegate.cpp
	layerlistwidget.cpp layerwidget.cpp
	core/layermimedata.cpp
	ora/zipfile.cpp ora/orawriter.cpp ora/orareader.cpp
	ora/zip.c ora/unzip.c ora/ioapi.c
)

set ( UIs ui/brushsettings.ui ui/simplesettings.ui ui/colordialog.ui
	ui/newdialog.ui ui/hostdialog.ui ui/joindialog.ui ui/logindialog.ui
	ui/chatbox.ui ui/palettebox.ui ui/colorbox.ui
	ui/settings.ui ui/navibox.ui ui/textsettings.ui ui/pensettings.ui
	ui/erasersettings.ui )

qt5_wrap_ui( UI_Headers ${UIs} )
qt5_add_resources( QtResource ui/resources.qrc )

# non-qt stuff

set ( EXETYPE "" )
set ( RES "" )
if ( WIN32 )
	if ( NOT DEBUG )
		set ( EXETYPE WIN32 )
	endif ( NOT DEBUG )
	
	generate_win32_resource (
		Win32Resource
		"DrawPile client" # full name
		${CLIENTNAME} # internal name
		"DrawPile client" 
                ${WEBSITE}
		"2006-2013 Calle Laakkonen"
                ${DRAWPILE_VERSION}
	)
endif ( WIN32 )

if ( RELEASE )
	generate_final ( SOURCES ${SOURCES} )
	#generate_final ( MOC_Sources ${MOC_Sources} )
endif ( RELEASE )

link_libraries(
	${DPSHAREDLIB}
	${QT_LIBRARIES}
	${ZLIB_LIBRARIES}
)

IF ( APPLE )
        SET(MACOSX_BUNDLE_BUNDLE_NAME ${CLIENTNAME})
        # set how it shows up in the Info.plist file
        SET(MACOSX_BUNDLE_ICON_FILE "drawpile.icns")
        # set the bundle identifier (REQUIRED, or some strange GUI bugs may appear)
        SET(MACOSX_BUNDLE_GUI_IDENTIFIER "org.numediart.${CLIENTNAME}")
        SET(MACOSX_BUNDLE_INFO_STRING "${CLIENTNAME}")
        SET(MACOSX_BUNDLE_LONG_VERSION_STRING "${CLIENTNAME}")
        SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "${CLIENTNAME}")
        SET(MACOSX_BUNDLE_BUNDLE_VERSION "${DRAWPILE_VERSION}")
        SET(MACOSX_BUNDLE_COPYRIGHT "Copyright DrawPile team")
        # set where in the bundle to put the icns file
        SET_SOURCE_FILES_PROPERTIES("${CMAKE_CURRENT_SOURCE_DIR}/ui/icons/drawpile.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        # include the icns file in the target
        SET(${SOURCES} ${SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/ui/icons/drawpile.icns")
ENDIF()

add_executable (
	drawpile
	${EXETYPE}
	MACOSX_BUNDLE
	${MACOSX_BUNDLE_INFO_PLIST}
	${SOURCES}
	${QtResource}
	${Win32Resource}
	#${MOC_Sources}
	${UI_Headers} # required here for ui_*.h generation
)

qt5_use_modules(drawpile Widgets Network Xml)

target_link_libraries(drawpile ${DPSHAREDLIB} ${ZLIB_LIBRARIES})
if ( WIN32 )
        target_link_libraries (drawpile ws2_32 )
endif ()

set_target_properties (
	drawpile
	PROPERTIES
        VERSION ${DRAWPILE_VERSION}
	PROJECT_LABEL drawpile-client
)

if ( WIN32 )
	install ( TARGETS ${CLIENTNAME} DESTINATION . )
else ( WIN32 )
	install ( TARGETS ${CLIENTNAME} DESTINATION bin )
endif ( WIN32 )

if (NOT DEBUG)
	if (APPLE)
		#strip_exe ( ${CLIENTNAME}.app/Contents/MacOS/${CLIENTNAME} )
		# Install the icon
		INSTALL(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/icons/drawpile.icns" DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/${CLIENTNAME}.app/Contents/Resources COMPONENT ${CLIENTNAME})

		# Install needed Qt plugins by copying directories from the qt installation
		INSTALL(DIRECTORY "${QT_PLUGINS_DIR}/imageformats" DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/${CLIENTNAME}.app/Contents/PlugIns COMPONENT ${CLIENTNAME})
		file(GLOB_RECURSE QTPLUGINS ${QT_PLUGINS_DIR}/imageformats/*.dylib)
		STRING(REGEX REPLACE "${QT_PLUGINS_DIR}" "${CMAKE_INSTALL_PREFIX}/bin/${CLIENTNAME}.app/Contents/PlugIns" QTPLUGINS "${QTPLUGINS}")

		# install a qt.conf file
		INSTALL(CODE "
 	   		file(WRITE \"\${CMAKE_INSTALL_PREFIX}/bin/${CLIENTNAME}.app/Contents/Resources/qt.conf\" \"[Paths]\nPlugins = plugins\")
 	   		" COMPONENT ${CLIENTNAME})
		FIND_FILE(QT_MENU_NIB qt_menu.nib PATHS /opt/local PATH_SUFFIXES Library/Frameworks/QtGui.framework/Resources lib/Resources)
		IF(${QT_MENU_NIB} EQUAL QT_MENU_NIB-NOTFOUND)
			MESSAGE(FATAL_ERROR "Couldn't find qt_menu.nib")
		ENDIF()
		INSTALL(DIRECTORY "${QT_MENU_NIB}" DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/${CLIENTNAME}.app/Contents/Resources COMPONENT ${CLIENTNAME})
		INSTALL(CODE "
    			include(BundleUtilities)
    			fixup_bundle(\"${CMAKE_INSTALL_PREFIX}/bin/${CLIENTNAME}.app\" \"${QTPLUGINS}\" \"\")
    			" COMPONENT ${CLIENTNAME}
		)
	else ()
		strip_exe ( ${CLIENTNAME} )
	endif()
endif (NOT DEBUG)

