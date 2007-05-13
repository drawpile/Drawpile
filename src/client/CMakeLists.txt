# client

set ( FULLNAME "DrawPile client" )
set ( INTERNALNAME "drawpile" )
set ( COPYRIGHT "Copyright (C) 2006,2007 Calle Laakkonen" )
set ( DESCRIPTION "DrawPile client" )
set ( FILE_COMMENT "http://drawpile.sourceforge.net/" )

set ( VERSION_MAJOR 0 )
set ( VERSION_MINOR 5 )
set ( VERSION_BUG 0 )

if ( WIN32 )
	set ( ORIGFILENAME "${INTERNALNAME}.exe" )
else ( WIN32 )
	set ( ORIGFILENAME "${INTERNALNAME}" )
endif ( WIN32 )

# Qt stuff

# enable Qt Network
set ( QT_USE_QTNETWORK ON )

find_package ( Qt4 REQUIRED )

include ( ${QT_USE_FILE} )

# files

set (
	Headers
	mainwindow.h netstatus.h dualcolorbutton.h editorview.h
	toolsettingswidget.h colordialog.h brushpreview.h colortriangle.h
	board.h controller.h layer.h brush.h user.h tools.h toolsettings.h
	interfaces.h boardeditor.h gradientslider.h colorbutton.h
	newdialog.h point.h imageselector.h hostdialog.h joindialog.h network.h
	networkprivate.h hoststate.h sessionstate.h logindialog.h preview.h
	localserver.h popupmessage.h userlistwidget.h userlistmodel.h chatwidget.h
	recentfiles.h sessioninfo.h sessionlistmodel.h icons.h mandatoryfields.h
	version.h palette.h localpalette.h palettewidget.h palettebox.h
	colorbox.h
)

set (
	Sources
	main.cpp mainwindow.cpp netstatus.cpp dualcolorbutton.cpp
	editorview.cpp toolsettingswidget.cpp colordialog.cpp brushpreview.cpp
	colortriangle.cpp board.cpp controller.cpp layer.cpp brush.cpp user.cpp
	tools.cpp toolsettings.cpp boardeditor.cpp
	gradientslider.cpp colorbutton.cpp newdialog.cpp imageselector.cpp
	hostdialog.cpp joindialog.cpp network.cpp hoststate.cpp sessionstate.cpp
	logindialog.cpp preview.cpp localserver.cpp popupmessage.cpp
	userlistwidget.cpp userlistmodel.cpp chatwidget.cpp recentfiles.cpp
	sessioninfo.cpp sessionlistmodel.cpp icons.cpp mandatoryfields.cpp
	localpalette.cpp palettewidget.cpp palettebox.cpp colorbox.cpp
)

set ( UIs ui/brushsettings.ui ui/simplesettings.ui ui/colordialog.ui
	ui/newdialog.ui ui/hostdialog.ui ui/joindialog.ui ui/logindialog.ui
	ui/chatbox.ui ui/sessiondialog.ui ui/palettebox.ui ui/colorbox.ui )

# .MOC

Qt4_automoc ( ${Sources} )

# .UI

set ( TEMP ${CMAKE_CURRENT_BINARY_DIR} ) # backup binary dir
set ( CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
Qt4_wrap_UI ( UI_Headers ${UIs} )
set ( CMAKE_CURRENT_BINARY_DIR ${TEMP} ) # restore binary dir

# .qrc

Qt4_add_resources ( Resource ui/resources.qrc )

# ?

Qt4_wrap_CPP ( MOC_Sources ${Headers} )

# non-qt4 stuff

if (NOT DEBUG)
	add_definitions(-DQT_NO_DEBUG)
	
	strip_exe ( drawpile )
endif ( NOT DEBUG )

set ( EXETYPE "" )

set ( RES "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	if ( NOT DEBUG )
		set ( EXETYPE WIN32 )
	endif ( NOT DEBUG )
	
	### generate resource file .o = ${WIN32RES}
	generate_win32_resource()
endif ( WIN32 )

link_libraries( libdrawpile ) # DP shared library

if ( ACCEPT_MTHREADS )
	set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthreads" )
endif ( ACCEPT_MTHREADS )

add_executable (
	${INTERNALNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${Sources}
	${UI_Headers} # required here for ui_*.h generation
	${Resource}
	${MOC_Sources}
	${WIN32RES}
)

target_link_libraries(
	drawpile
	${QT_LIBRARIES}
)

set_target_properties (
	drawpile
	PROPERTIES
	VERSION 0.5.0
	PROJECT_LABEL drawpile-client
)

install ( TARGETS ${CLIENTNAME} RUNTIME DESTINATION bin )
