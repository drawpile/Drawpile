# tray server

set ( FULLNAME "DrawPile Tray Server" )
set ( INTERNALNAME "drawpile-tray-srv" )
set ( COPYRIGHT "Copyright (C) 2007 M.K.A." )
set ( DESCRIPTION ${FULLNAME} )
set ( FILE_COMMENT "http://drawpile.sourceforge.net/" )

set ( VERSION_MAJOR 0 )
set ( VERSION_MINOR 1 )
set ( VERSION_BUG 0 )

if ( WIN32 )
	set ( ORIGFILENAME "${INTERNALNAME}.exe" )
else ( WIN32 )
	set ( ORIGFILENAME "${INTERNALNAME}" )
endif ( WIN32 )

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
	strip_exe ( drawpile-tray-srv )
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

generate_win32_resource( Win32Resource )

add_executable (
	${INTERNALNAME}
	${EXETYPE}
	MACOSX_BUNDLE
	${SSOURCES}
	${MOC_Sources}
	${Win32Resource}
	${QtResource}
)

target_link_libraries ( ${INTERNALNAME} ${QT_LIBRARIES} )
target_link_libraries ( ${INTERNALNAME} libdrawpile ) # DP shared library
target_link_libraries ( ${INTERNALNAME} dpserverlib ) # Server class lib
target_link_libraries ( ${INTERNALNAME} eventslib ) # Events lib, required by Server class

install ( TARGETS ${INTERNALNAME} RUNTIME DESTINATION bin )
