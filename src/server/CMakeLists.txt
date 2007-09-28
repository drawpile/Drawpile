# src/server/

# include some stuff
include ( CheckIncludeFile )
include ( CheckFunctionExists )

### For deflate extension

if ( USE_URANDOM )
	find_file (
		HAVE_URANDOM
		urandom
		PATHS /dev
		NO_DEFAULT_PATH
	)
endif ( USE_URANDOM )

set ( HAVE_ZLIB OFF )
if ( SERVERLIB AND DEFLATE )
	find_package ( ZLIB )
	if ( ZLIB_FOUND )
		set ( HAVE_ZLIB ON )
		link_libraries ( ${ZLIB_LIBRARIES} )
	else ( ZLIB_FOUND )
		message ( STATUS "ZLib not found!" )
	endif ( ZLIB_FOUND )
endif ( SERVERLIB AND DEFLATE )

### fork()

if ( SERVER )
	check_function_exists ( fork1 HAVE_FORK1 )
	
	if ( NOT HAVE_FORK1 )
		check_function_exists ( fork HAVE_FORK )
	endif ( NOT HAVE_FORK1 )
	
	if ( HAVE_FORK OR HAVE_FORK1 )
		link_libraries ( pthread )
	endif ( HAVE_FORK OR HAVE_FORK1 )
endif ( SERVER )

# persistent sessions

set ( PERSISTENT_SESSIONS OFF )
if ( PERSISTENT )
	message ( STATUS "Persistent sessions enabled" )
	set ( PERSISTENT_SESSIONS ON )
endif ( PERSISTENT )

# IP stack config
set ( IPV_DUAL_STACK OFF )
if ( IPV4 AND IPV6 )
	check_include_file ( mstcpip.h HAVE_MSTCPIP )
	#if ( WIN32 )
		#if ( HAVE_MSTCPIP )
			#message ( STATUS "Dual IP stack enabled!" )
			#set ( IPV_DUAL_STACK ON )
		#endif ( HAVE_MSTCPIP )
	#endif ( WIN32 )
	if ( NOT IPV_DUAL_STACK )
		message ( FATAL_ERROR "Dual IP stack not supported on current system!" )
	endif ( NOT IPV_DUAL_STACK )
	message ( "IPv4/IPv6 dual-stack enabled" )
else ( IPV4 AND IPV6 )
	if ( IPV4 )
		message ( STATUS "IPv4 stack enabled" )
	else ( IPV4 )
		message ( STATUS "IPv6 stack enabled" )
	endif ( IPV4 )
endif ( IPV4 AND IPV6 )

if ( NOT IPV4 AND NOT IPV6 )
	message ( FATAL_ERROR "Both IPv4/IPv6 stacks disabled!" )
endif ( NOT IPV4 AND NOT IPV6 )

# Layers
set ( LAYER_SUPPORT OFF )
if ( LAYERS )
	message ( STATUS "Experimental layer code enabled." )
	set ( LAYER_SUPPORT ON )
endif ( LAYERS )

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
		user.cpp
		session.cpp
		network.cpp
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
		${SRVLIB}
		${SRV_SOURCES}
	)
	
	if ( SHARED )
		strip_lib ( ${SRVLIB} )
		if ( WIN32 )
			install ( TARGETS ${SRVLIB} RUNTIME DESTINATION . ) 
		else ( WIN32 )
			install ( TARGETS ${SRVLIB} LIBRARY DESTINATION lib ) # ?
		endif ( WIN32 )
	endif ( SHARED )
	
	target_link_libraries(
		${SRVLIB}
		${DPSHAREDLIB} # DP shared library
		${EVENTSLIB} # EV lib
	)
	
	# Console executable
	set_target_properties (
		${SRVLIB}
		PROPERTIES
		OUTPUT_NAME ${SRVNAME}
		VERSION 0.5
		SOVERSION 0.5
		PROJECT_LABEL ${SRVLIB}
	)
endif ( SERVERLIB )

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
	endif ( WIN32 )
	
	add_executable (
		${SRVNAME}
		dpsrv.cpp
		${Win32Resource}
	)
	
	set_target_properties (
		${SRVNAME}
		PROPERTIES
		VERSION 0.5.0
		PROJECT_LABEL drawpile-server
	)
	
	target_link_libraries(
		${SRVNAME}
		${DPSHAREDLIB}
		${SRVLIB} # DP shared library
		${EVENTSLIB} # EV lib
	)
	
	if ( WIN32 )
		install ( TARGETS ${SRVNAME} RUNTIME DESTINATION . )
	else ( WIN32 )
		install ( TARGETS ${SRVNAME} RUNTIME DESTINATION bin )
	endif ( WIN32 )
	
	if ( NOT DEBUG )
		strip_exe ( ${SRVNAME} )
	endif ( NOT DEBUG )
endif ( SERVER )
