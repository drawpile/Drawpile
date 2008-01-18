# src/server/

# include some stuff
include ( CheckIncludeFile )
include ( CheckFunctionExists )

### For deflate extension

set ( HAVE_ZLIB false )
if ( SERVERLIB AND DEFLATE )
	find_package ( ZLIB )
	if ( ZLIB_FOUND )
		set ( HAVE_ZLIB true )
		link_libraries ( ${ZLIB_LIBRARIES} )
	else ( )
		message ( STATUS "ZLib not found!" )
	endif ( )
endif ( )

### fork()

if ( SERVER )
	check_function_exists ( fork1 HAVE_FORK1 )
	
	if ( NOT HAVE_FORK1 )
		check_function_exists ( fork HAVE_FORK )
	endif ( )
	
	if ( HAVE_FORK OR HAVE_FORK1 )
		link_libraries ( pthread )
	endif ( )
endif ( )

# persistent sessions

set ( PERSISTENT_SESSIONS false )
if ( PERSISTENT )
	message ( STATUS "Persistent sessions enabled" )
	set ( PERSISTENT_SESSIONS true )
endif ( )

# IP stack config
set ( IPV_DUAL_STACK false )
if ( IPV4 AND IPV6 )
	check_include_file ( mstcpip.h HAVE_MSTCPIP )
	if ( WIN32 )
		if ( HAVE_MSTCPIP )
			message ( STATUS "Dual IP stack enabled!" )
			set ( IPV_DUAL_STACK true )
		endif ( )
	endif ( )
	if ( NOT IPV_DUAL_STACK )
		message ( FATAL_ERROR "Dual IP stack not supported on current system!" )
	else ( )
		message ( STATUS "IPv4/IPv6 dual-stack enabled" )
	endif ( )
else ( IPV4 AND IPV6 )
	if ( IPV4 )
		message ( STATUS "IPv4 stack enabled" )
	else ( )
		message ( STATUS "IPv6 stack enabled" )
	endif ( )
endif ( )

if ( NOT IPV4 AND NOT IPV6 )
	message ( FATAL_ERROR "Both IPv4/IPv6 stacks disabled!" )
endif ( )

# Layers
set ( LAYER_SUPPORT false )
if ( LAYERS )
	message ( STATUS "Experimental layer code enabled." )
	set ( LAYER_SUPPORT true )
endif ( )

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
add_definitions ( -DHAVE_SRVCONFIG )

if ( WIN32 )
	link_libraries ( ws2_32 )
endif ( )

add_subdirectory ( ev/ ) # events lib

if ( SERVERLIB )
	set (
		SRV_SOURCES
		buffer.cpp
		session_data.cpp
		user.cpp
		session.cpp
		network.cpp
		address.cpp
		socket.cpp
		server.cpp
		ref_counted.cpp
	)
	
	if ( RELEASE )
		generate_final ( SRV_SOURCES ${SRV_SOURCES} )
	endif ( )
	
	# Create library
	add_library (
		${SRVLIB}
		${SRV_SOURCES}
	)
	
	if ( SHARED )
		strip_lib ( ${SRVLIB} )
		if ( WIN32 )
			install ( TARGETS ${SRVLIB} RUNTIME DESTINATION . ) 
		else ( )
			install ( TARGETS ${SRVLIB} LIBRARY DESTINATION lib ) # ?
		endif ( )
	endif ( )
	
	target_link_libraries(${SRVLIB} ${DPSHAREDLIB} ${EVENTSLIB} )
	
	# Console executable
	set_target_properties (
		${SRVLIB}
		PROPERTIES
		OUTPUT_NAME ${SRVNAME}
		VERSION 0.5
		SOVERSION 0.5
		PROJECT_LABEL ${SRVLIB}
	)
endif ( )

if ( SERVER )
	if ( WIN32 )
		generate_win32_resource (
			Win32Resource
			"DrawPile server" # full name
			${SRVNAME} # internal name
			"DrawPile server"  # comment
			"http://drawpile.sourceforge.net/" # description
			"2006,2007 M.K.A." # copyright holder
			0 5 0 # version
		)
	endif ( )
	
	add_executable (
		${SRVNAME}
		dpsrv.cpp
		${Win32Resource}
	)
	
	target_link_libraries( ${SRVNAME} ${DPSHAREDLIB} ${EVENTSLIB} ${SRVLIB} )
	
	set_target_properties (
		${SRVNAME}
		PROPERTIES
		VERSION 0.5.0
		PROJECT_LABEL drawpile-server
	)
	
	if ( WIN32 )
		install ( TARGETS ${SRVNAME} RUNTIME DESTINATION . )
	else ( )
		install ( TARGETS ${SRVNAME} RUNTIME DESTINATION bin )
	endif ( )
	
	if ( NOT DEBUG )
		strip_exe ( ${SRVNAME} )
	endif ( )
endif ( )
