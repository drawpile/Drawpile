# src/shared/

set ( INTERNALNAME "lib${PROJECT_NAME}" )

#if (WIN32)
#	link_libraries (ws2_32) # winsock2
#endif (WIN32)

set (
	SHR_SOURCES
	protocol.cpp
	protocol.helper.cpp
	SHA1.cpp
)

if ( RELEASE )
	InSourceDir ( SHR_SOURCES ${SHR_SOURCES} )
	generate_final ( SHR_SOURCES ${SHR_SOURCES} )
endif ( RELEASE )

add_library (
	${INTERNALNAME}
	${SHR_SOURCES}
)

set_target_properties (
	${INTERNALNAME}
	PROPERTIES
	OUTPUT_NAME drawpile
	VERSION 0.4.0
	SOVERSION 0.4.0
	PROJECT_LABEL drawpile-library
)

if ( SHARED )
	install ( TARGETS ${INTERNALNAME} LIBRARY DESTINATION lib )
endif ( SHARED )

if ( OPENSSL )
	find_package ( OpenSSL QUIET )
	if ( OPENSSL_FOUND )
		set ( HAVE_OPENSSL ON )
		message ( STATUS "OpenSSL libraries found!" )
		target_link_libraries ( ${INTERNALNAME} ${OPENSSL_LIBRARIES} )
	endif ( OPENSSL_FOUND )
endif ( OPENSSL )

configure_file ( config.h.cmake config.h )
