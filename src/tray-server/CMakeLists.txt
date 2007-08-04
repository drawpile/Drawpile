# tray server

set ( TRAYSERVERNAME "${PROJECT_NAME}-tray-srv" )

# Qt stuff
include ( ${QT_USE_FILE} )

# files

set (
	SSOURCES
	main.cpp
	traymenu.cpp
	srvthread.cpp
	configdialog.cpp
	statusdialog.cpp
	network.cpp
	strings.cpp
)

Qt4_wrap_CPP (
	MOC_Sources
	traymenu.h
	srvthread.h
	statusdialog.h
	configdialog.h
)

#Qt4_automoc ( ${MOC_Sources} )
Qt4_add_resources ( QtResource systray.qrc )

add_definitions(${QT_DEFINITIONS})
if ( NOT DEBUG )
	add_definitions( -DQT_NO_DEBUG )
	add_definitions( -DNDEBUG )
endif ( NOT DEBUG )

set ( EXETYPE "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	if ( NOT DEBUG )
		set ( EXETYPE WIN32 )
	endif ( NOT DEBUG )
endif ( WIN32 )

if ( RELEASE )
	InSourceDir ( SSOURCES ${SSOURCES} )
	generate_final ( SSOURCES ${SSOURCES} )
endif ( RELEASE )

if ( WIN32 )
	generate_win32_resource (
		Win32Resource
		"DrawPile Tray Server" # full name
		${TRAYSERVERNAME} # internal name
		"DrawPile Tray Server"  # comment
		"http://drawpile.sourceforge.net/" # description
		"2007 M.K.A." # copyright holder
		0 1 0 # version
	)
endif ( WIN32 )

link_libraries (
	${QT_LIBRARIES}
	libdrawpile # DP shared library
	dpserverlib # Server class lib
	eventslib # Events lib, required by Server class
)

add_executable (
	${TRAYSERVERNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${SSOURCES}
	${MOC_Sources}
	${Win32Resource}
	${QtResource}
)

install ( TARGETS ${TRAYSERVERNAME} RUNTIME DESTINATION bin )

if ( NOT DEBUG )
	strip_exe ( ${TRAYSERVERNAME} )
endif ( NOT DEBUG )
