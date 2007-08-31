# src/server/

set ( SERVERNAME "${PROJECT_NAME}-srv" )
set ( SERVERLIB "dpserverlib" )

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
	else ( DEFLATE )
		set ( HAVE_ZLIB OFF )
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

# persistent sessions

set ( PERSISTENT_SESSIONS OFF )
if ( PERSISTENT )
	set ( PERSISTENT_SESSIONS ON )
endif ( PERSISTENT )

# config

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_SRVCONFIG )

if ( WIN32 )
	link_libraries ( ws2_32 )
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
		server.cpp
		ref_counted.cpp
	)
	
	if ( RELEASE )
		InSourceDir( SRV_SOURCES ${SRV_SOURCES} )
		generate_final ( SRV_SOURCES ${SRV_SOURCES} )
	endif ( RELEASE )
	
	# Create library
	add_library (
		${SERVERLIB}
		${SRV_SOURCES}
	)

	target_link_libraries(
		${SERVERLIB}
		libdrawpile # DP shared library
		eventslib # EV lib
	)
	
	# Console executable
	set_target_properties (
		${SERVERLIB}
		PROPERTIES
		OUTPUT_NAME dpserver
		VERSION 0.5
		SOVERSION 0.5
		PROJECT_LABEL ${SERVERLIB}
	)
endif ( SERVERLIB )

if ( SERVER )
	if ( WIN32 )
		generate_win32_resource (
			Win32Resource
			"DrawPile server" # full name
			${SERVERNAME} # internal name
			"DrawPile server"  # comment
			"http://drawpile.sourceforge.net/" # description
			"2006,2007 M.K.A." # copyright holder
			0 5 0 # version
		)
	endif ( WIN32 )
	
	add_executable (
		${SERVERNAME}
		MACOSX_BUNDLE
		dpsrv.cpp
		${Win32Resource}
	)
	
	add_dependencies ( ${SERVERNAME} ${SERVERLIB} eventslib libdrawpile )
	
	set_target_properties (
		${SERVERNAME}
		PROPERTIES
		VERSION 0.5.0
		PROJECT_LABEL drawpile-server
	)
	
	target_link_libraries(
		${SERVERNAME}
		${SERVERLIB} # DP shared library
		eventslib # EV lib
	)
	
	install ( TARGETS ${SERVERNAME} RUNTIME DESTINATION bin )
	
	if ( NOT DEBUG )
		strip_exe ( ${SERVERNAME} )
	endif ( NOT DEBUG )
endif ( SERVER )
