# client

set ( FULLNAME "DrawPile Client" )
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

#if ( PROCESS_WIDGETS )
#	add_subdirectory ( widgets/ )
#endif ( PROCESS_WIDGETS )

# files

set (
	Client_Headers
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
	Client_Sources
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

set ( Client_Resources ui/resources.qrc )
set ( Client_UIs ui/brushsettings.ui ui/simplesettings.ui ui/colordialog.ui
	ui/newdialog.ui ui/hostdialog.ui ui/joindialog.ui ui/logindialog.ui
	ui/chatbox.ui ui/sessiondialog.ui ui/palettebox.ui ui/colorbox.ui )

# .MOC

set ( TEMP ${CMAKE_CURRENT_BINARY_DIR} ) # backup binary dir
set ( CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR} )

Qt4_automoc ( ${Client_Sources} )

# .UI

Qt4_wrap_UI ( Client_UI_Headers ${Client_UIs} )

Qt4_add_resources ( Client_Resource_Sources ${Client_Resources} )

Qt4_wrap_CPP ( MOC_Sources ${Client_Headers} )

set ( CMAKE_CURRENT_BINARY_DIR ${TEMP} ) # restore binary dir

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
	
	### generate resource file .o
	set ( SourceRES ${CMAKE_CURRENT_SOURCE_DIR}/client.rc )
	dump_resource ( ${SourceRES} )
	
	set ( RES ${CMAKE_CURRENT_BINARY_DIR}/client.resource.o )
	add_custom_command(
		OUTPUT ${RES}
		COMMAND windres ${SourceRES} ${RES}
	)
	
	get_directory_property (
		CLEANF
		ADDITIONAL_MAKE_CLEAN_FILES
	)
	
	set_directory_properties (
		PROPERTIES
		ADDITIONAL_MAKE_CLEAN_FILES
		"${CLEANF} client.resource.o"
	)
endif ( WIN32 )

link_libraries( libdrawpile ) # DP shared library

if ( ACCEPT_MTHREADS )
	set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthreads" )
endif ( ACCEPT_MTHREADS )

add_executable (
	drawpile
	${EXETYPE}
	MACOSX_BUNDLE
	${RES}
	${Client_Sources}
	${Client_Headers}
	${Client_UI_Headers}
	${Client_Resource_Sources}
	${MOC_Sources}
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
