# src/server/

if ( SERVER )
	set ( FULLNAME "DrawPile server" )
	set ( INTERNALNAME "${PROJECT_NAME}-srv" )
	set ( COPYRIGHT "Copyright (C) 2006,2007 M.K.A." )
	set ( DESCRIPTION ${FULLNAME} )
	set ( FILE_COMMENT "http://drawpile.sourceforge.net/" )
	
	set ( VERSION_MAJOR 0 )
	set ( VERSION_MINOR 5 )
	set ( VERSION_BUG 0 )
	
	if ( WIN32 )
		set ( ORIGFILENAME "${INTERNALNAME}.exe" )
	else ( WIN32 )
		set ( ORIGFILENAME "${INTERNALNAME}" )
	endif ( WIN32 )
endif ( SERVER )

# include some stuff

include ( CheckIncludeFile )
include ( CheckFunctionExists )

### For deflate extension

if ( SERVERLIB )
	if ( DEFLATE )
		find_package ( ZLIB )
		if ( ZLIB_FOUND )
			set ( HAVE_ZLIB ON )
			link_libraries ( ${ZLIB_LIBRARIES} )
		else ( ZLIB_FOUND )
			check_include_file ( zlib.h HAVE_ZLIB )
			if ( HAVE_ZLIB )
				link_libraries ( "z" )
			else ( HAVE_ZLIB )
				message ( STATUS "ZLIB disabled!" )
				set ( DEFLATE OFF )
			endif ( HAVE_ZLIB )
		endif ( ZLIB_FOUND )
	endif ( DEFLATE )
endif ( SERVERLIB )

### fork()

if ( SERVER )
	check_function_exists ( fork HAVE_FORK )
	check_function_exists ( fork1 HAVE_FORK1 )
	
	if ( HAVE_FORK1 )
		set ( HAVE_FORK OFF )
		link_libraries ( pthread )
	endif ( HAVE_FORK1 )
	
	if ( HAVE_FORK )
		link_libraries ( pthread )
	endif ( HAVE_FORK )
endif ( SERVER )

# scatter-gather sockets

check_function_exists ( sendmsg HAVE_SENDMSG )
check_function_exists ( recvmsg HAVE_RECVMSG )

set ( SC_SOCKETS OFF )
if ( HAVE_SENDMSG )
	if ( HAVE_RECVMSG )
		set ( SC_SOCKETS ON )
	endif ( HAVE_RECVMSG )
endif ( HAVE_SENDMSG )

if ( PERSISTENT )
	set ( PERSISTENT_SESSIONS ON )
endif ( PERSISTENT )

# config

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_SRVCONFIG )

set ( WIN32RES "" )
if ( WIN32 )
	link_libraries ( ws2_32 )
	
	if ( SERVER )
		### generate resource file .o = ${WIN32RES}
		generate_win32_resource()
	endif ( SERVER )
endif ( WIN32 )

add_subdirectory ( ev/ ) # events lib

if ( SERVERLIB )
	set (
		SRV_SOURCES
		buffer.cpp
		session_data.cpp
		layer_data.cpp
		user.cpp
		session.cpp
		net.cpp
		address.cpp
		socket.cpp
		#dpsrv.cpp
		server.cpp
	)
	
	if ( RELEASE )
		generate_all_cpp ( "${SRV_SOURCES}" )
		set ( SRV_SOURCES auto_all.cpp )
	endif ( RELEASE )
	
	# Create library
	add_library (
		dpserverlib
		${SRV_SOURCES}
	)

	target_link_libraries(
		dpserverlib
		libdrawpile # DP shared library
		eventslib # EV lib
	)
	
	# Console executable
	set_target_properties (
		dpserverlib
		PROPERTIES
		OUTPUT_NAME dpserver
		VERSION 0.5
		SOVERSION 0.5
		PROJECT_LABEL dpserverlib
	)
endif ( SERVERLIB )

if ( SERVER )
	if ( NOT DEBUG )
		strip_exe ( ${INTERNALNAME} )
	endif ( NOT DEBUG )
	
	add_executable (
		${INTERNALNAME}
		MACOSX_BUNDLE
		dpsrv.cpp
		${WIN32RES}
	)
	
	add_dependencies ( drawpile-srv dpserverlib eventslib libdrawpile )
	
	set_target_properties (
		${INTERNALNAME}
		PROPERTIES
		VERSION 0.5.0
		PROJECT_LABEL drawpile-server
	)
	
	target_link_libraries(
		${INTERNALNAME}
		dpserverlib # DP shared library
		libdrawpile # DP shared library
		eventslib # EV lib
	)
	
	install ( TARGETS ${INTERNALNAME} RUNTIME DESTINATION bin )
	
	if ( NOT DEBUG )
		strip_exe ( ${INTERNALNAME} )
	endif ( NOT DEBUG )
endif ( SERVER )
