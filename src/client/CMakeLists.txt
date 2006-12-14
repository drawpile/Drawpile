set ( EXECNAME drawpile )

# Qt stuff

find_package ( Qt4 REQUIRED )

# Qt_definitions ( foo )

include_directories (
	${QT_INCLUDE_DIR}
	${QT_QTGUI_INCLUDE_DIR}
	${QT_QTCORE_INCLUDE_DIR}
)

include ( ${QT_USE_FILE} )

#set ( QT_USE_QTASSISTANT TRUE )
#set ( QT_USE_QTDESIGNER TRUE )
#set ( QT_USE_QTMAIN TREU )
#set ( QT_USE_QTNETWORK TRUE )
#set ( QT_USE_QTOPENGL TRUE )
#set ( QT_USE_QTXML TRUE )

#message ( "DEVNOTE | Qt found at: " ${QT_INCLUDE_DIR} )

# subdirs

#if ( PROCESS_WIDGETS )
#	add_subdirectory ( widgets/ )
#endif ( PROCESS_WIDGETS )

# files

set (
	Client_Headers
	mainwindow.h netstatus.h hostlabel.h dualcolorbutton.h editorview.h
	toolsettingswidget.h colordialog.h brushpreview.h colortriangle.h
	board.h controller.h layer.h brush.h user.h tools.h toolsettings.h
	interfaces.h boardeditor.h aboutdialog.h gradientslider.h
)

set (
	Client_Sources
	main.cpp mainwindow.cpp netstatus.cpp hostlabel.cpp dualcolorbutton.cpp
	editorview.cpp toolsettingswidget.cpp colordialog.cpp brushpreview.cpp
	colortriangle.cpp board.cpp controller.cpp layer.cpp brush.cpp user.cpp
	tools.cpp toolsettings.cpp interfaces.cpp boardeditor.cpp aboutdialog.cpp
	gradientslider.cpp
)

set ( Client_Resources ui/resources.qrc )
set ( Client_UIs ui/brushsettings.ui ui/colordialog.ui ui/about.ui )

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( WIN32 )

link_libraries( dpshared )

# .MOC

Qt4_automoc ( ${Client_Sources} )

# .UI

Qt4_wrap_UI ( Client_UI_Headers ${Client_UIs} )

Qt4_add_resources ( Client_Resource_Sources ${Client_Resources} )

Qt4_wrap_CPP ( MOC_Sources ${Client_Headers} )

if (WIN32)
	set ( EXEFORMAT WIN32 )
endif (WIN32)

if (MACOSX)
	set ( EXEFORMAT MACOSX_BUNDLE )
endif (MACOSX)

add_executable (
	${EXECNAME}
	${EXEFORMAT}
	${Client_Sources}
	${Client_Headers}
	${Client_UI_Headers}
	${Client_Resource_Sources}
	${MOC_Sources}
)

include (AddFileDependencies)
add_file_dependencies ( main.cpp src/shared )

target_link_libraries(
	${EXECNAME}
	${QT_LIBRARIES}
)
