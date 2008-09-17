# client

# Qt stuff
include ( ${QT_USE_FILE} )

# files

set (
	HEADERS
	mainwindow.h netstatus.h dualcolorbutton.h editorview.h
	toolsettingswidget.h colordialog.h brushpreview.h colortriangle.h
	board.h controller.h gradientslider.h colorbutton.h
	newdialog.h imageselector.h hostdialog.h joindialog.h
	hoststate.h sessionstate.h logindialog.h
	localserver.h popupmessage.h userlistwidget.h userlistmodel.h chatwidget.h
	mandatoryfields.h palettewidget.h palettebox.h
	colorbox.h settingsdialog.h
	navigator.h
)

# unused headers because they do not create moc_* files ()
set (
	UNUSED_HEADERS # no QOBJECT ?
	version.h user.h interfaces.h preview.h boardeditor.h boarditem.h
	localpalette.h palette.h icons.h
	tools.h toolsettings.h recentfiles.h sessioninfo.h
	core/tile.h core/layer.h core/brush.h core/point.h core/rasterop.h
)

set (
	SOURCES
	main.cpp mainwindow.cpp netstatus.cpp dualcolorbutton.cpp
	editorview.cpp toolsettingswidget.cpp colordialog.cpp brushpreview.cpp
	colortriangle.cpp board.cpp controller.cpp boarditem.cpp user.cpp
	tools.cpp toolsettings.cpp boardeditor.cpp
	gradientslider.cpp colorbutton.cpp newdialog.cpp imageselector.cpp
	hostdialog.cpp joindialog.cpp hoststate.cpp sessionstate.cpp
	logindialog.cpp preview.cpp localserver.cpp popupmessage.cpp
	userlistwidget.cpp userlistmodel.cpp chatwidget.cpp recentfiles.cpp
	sessioninfo.cpp icons.cpp mandatoryfields.cpp
	localpalette.cpp palettewidget.cpp palettebox.cpp colorbox.cpp
	settingsdialog.cpp palette.cpp navigator.cpp
	core/tile.cpp core/layer.cpp core/brush.cpp
)

set ( UIs ui/brushsettings.ui ui/simplesettings.ui ui/colordialog.ui
	ui/newdialog.ui ui/hostdialog.ui ui/joindialog.ui ui/logindialog.ui
	ui/chatbox.ui ui/palettebox.ui ui/colorbox.ui
	ui/settings.ui ui/navibox.ui )

# .UI

set ( TEMP ${CMAKE_CURRENT_BINARY_DIR} ) # backup binary dir
set ( CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
QT4_WRAP_UI ( UI_Headers ${UIs} )
set ( CMAKE_CURRENT_BINARY_DIR ${TEMP} ) # restore binary dir

# Qt Resource

Qt4_add_resources ( QtResource ui/resources.qrc )

# Qt4 MOC

Qt4_wrap_CPP ( MOC_Sources ${HEADERS} ) # generate moc_*.cxx
#Qt4_automoc ( ${MOC_Sources} ) # moc_*.cxx files ??

# non-qt4 stuff

add_definitions(${QT_DEFINITIONS})

if (NOT DEBUG)
	add_definitions(-DQT_NO_DEBUG)
endif ( NOT DEBUG )

set ( EXETYPE "" )
set ( RES "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	if ( NOT DEBUG )
		set ( EXETYPE WIN32 )
	endif ( NOT DEBUG )
	
	generate_win32_resource (
		Win32Resource
		"DrawPile client" # full name
		${CLIENTNAME} # internal name
		"DrawPile client" 
		"http://drawpile.sourceforge.net/"
		"2006-2008 Calle Laakkonen"
		0 5 0
	)
endif ( WIN32 )

if ( RELEASE )
	generate_final ( SOURCES ${SOURCES} )
	#generate_final ( MOC_Sources ${MOC_Sources} )
endif ( RELEASE )

link_libraries(
	${DPSHAREDLIB}
	${QT_LIBRARIES}
)

add_executable (
	${CLIENTNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${SOURCES}
	${QtResource}
	${Win32Resource}
	${MOC_Sources}
	${UI_Headers} # required here for ui_*.h generation
)

set_target_properties (
	drawpile
	PROPERTIES
	VERSION 0.5.0
	PROJECT_LABEL drawpile-client
)

if ( WIN32 )
	install ( TARGETS ${CLIENTNAME} RUNTIME DESTINATION . )
else ( WIN32 )
	install ( TARGETS ${CLIENTNAME} RUNTIME DESTINATION bin )
endif ( WIN32 )

if (NOT DEBUG)
	strip_exe ( ${CLIENTNAME} )
endif (NOT DEBUG)
