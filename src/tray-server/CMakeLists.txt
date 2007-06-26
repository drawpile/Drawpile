# tray server

set ( FULLNAME "DrawPile tray server" )
set ( INTERNALNAME "drawpile-tray-srv" )
set ( COPYRIGHT "Copyright (C) 2007 M.K.A." )
set ( DESCRIPTION ${FULLNAME} )
set ( FILE_COMMENT "http://drawpile.sourceforge.net/" )

set ( VERSION_MAJOR 0 )
set ( VERSION_MINOR 1 )
set ( VERSION_BUG 0 )

# Qt stuff
include ( ${QT_USE_FILE} )

# files

set (
	SSOURCES
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

Qt4_automoc ( ${MOC_Sources} )
Qt4_add_resources ( QtResource systray.qrc )

if ( NOT DEBUG )
	add_definitions( -DQT_NO_DEBUG )
	add_definitions( -DNDEBUG )
	strip_exe ( drawpile-tray-srv )
endif ( NOT DEBUG )

set ( EXETYPE "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	if ( NOT DEBUG )
		set ( EXETYPE WIN32 )
	endif ( NOT DEBUG )
	
	### generate resource file .o = ${WIN32RES}
	generate_win32_resource()
endif ( WIN32 )

add_executable (
	${INTERNALNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${MOC_Sources}
	${SSOURCES}
	main.cpp
	${QtResource}
	${WIN32RES}
)

target_link_libraries ( ${INTERNALNAME} ${QT_LIBRARIES} )
target_link_libraries ( ${INTERNALNAME} libdrawpile ) # DP shared library
target_link_libraries ( ${INTERNALNAME} dpserverlib ) # Server class lib
target_link_libraries ( ${INTERNALNAME} eventslib ) # Events lib, required by Server class

install ( TARGETS ${INTERNALNAME} RUNTIME DESTINATION bin )
