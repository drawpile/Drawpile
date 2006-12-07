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

# add_subdirectory ( widgets/ )

# files

set ( C_HDRS
	mainwindow.h netstatus.h hostlabel.h dualcolorbutton.h editorview.h
	toolsettingswidget.h colordialog.h brushpreview.h colortriangle.h
	board.h controller.h layer.h brush.h user.h tools.h toolsettings.h
	interfaces.h boardeditor.h
)

set ( C_SRCS
	main.cpp mainwindow.cpp netstatus.cpp hostlabel.cpp dualcolorbutton.cpp
	editorview.cpp toolsettingswidget.cpp colordialog.cpp brushpreview.cpp
	colortriangle.cpp board.cpp controller.cpp layer.cpp brush.cpp user.cpp
	tools.cpp toolsettings.cpp interfaces.cpp boardeditor.cpp
)

set ( C_RES ui/resources.qrc )
set ( C_UIS ui/brushsettings.ui ui/colordialog.ui )

if ( WIN32 )
	# link_libraries ( ws2_32 )
endif ( WIN32 )

# link_libraries( dpshared )

# .MOC

Qt4_automoc ( ${C_SRCS} )

# .UI

Qt4_wrap_UI ( UI_HDRS ${C_UIS} )

Qt4_add_resources ( C_RES_SRCS ${C_RES} )

Qt4_wrap_CPP ( MOC_SRCS ${C_HDRS} )

add_executable (
	${EXECNAME}
	${C_SRCS}
	${C_HDRS}
	${UI_HDRS}
	${C_RES_SRCS}
	${MOC_SRCS}
)

target_link_libraries(
	${EXECNAME}
	${QT_LIBRARIES}
)
