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
	SOURCES
	main.cpp
	mainwindow.cpp 
	canvasscene.cpp
	canvasview.cpp
	canvasitem.cpp
	statetracker.cpp
	tools.cpp
	toolsettings.cpp
	annotationitem.cpp
	loader.cpp
	textloader.cpp
	net/client.cpp
	net/loopbackserver.cpp
	net/tcpserver.cpp
	net/utils.cpp
	net/login.cpp
	net/userlist.cpp
	net/layerlist.cpp
	net/serverthread.cpp
	utils/palette.cpp
	utils/mandatoryfields.cpp
	utils/recentfiles.cpp
	utils/icons.cpp 
	core/tile.cpp
	core/layer.cpp
	core/layerstack.cpp
	core/brush.cpp
	core/rasterop.cpp
	ora/zipfile.cpp
	ora/orawriter.cpp
	ora/orareader.cpp
	ora/zip.c
	ora/unzip.c
	ora/ioapi.c
	dialogs/colordialog.cpp 
	dialogs/newdialog.cpp 
	dialogs/hostdialog.cpp
	dialogs/joindialog.cpp
	dialogs/settingsdialog.cpp
	widgets/viewstatus.cpp
	widgets/palettewidget.cpp 
	widgets/popupmessage.cpp
	widgets/netstatus.cpp 
	widgets/qtcolortriangle.cpp 
	widgets/dualcolorbutton.cpp
	widgets/chatlineedit.cpp
	widgets/chatwidget.cpp 
	widgets/brushslider.cpp 
	widgets/colorbutton.cpp 
	widgets/gradientslider.cpp 
	widgets/imageselector.cpp
	widgets/brushpreview.cpp
	docks/toolsettingswidget.cpp 
	docks/palettebox.cpp 
	docks/navigator.cpp 
	docks/colorbox.cpp
	docks/userlistdock.cpp 
	docks/layerlistdock.cpp
	docks/layerlistdelegate.cpp
)

set (
	UIs
	ui/brushsettings.ui
	ui/simplesettings.ui
	ui/colordialog.ui
	ui/hostdialog.ui
	ui/joindialog.ui
	ui/palettebox.ui
	ui/colorbox.ui
	ui/settings.ui
	ui/navibox.ui
	ui/textsettings.ui
	ui/pensettings.ui
	ui/erasersettings.ui
	ui/layerbox.ui
	ui/userbox.ui
	ui/newdialog.ui
)

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

