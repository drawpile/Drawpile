# src/shared/

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
	${DPSHAREDLIB}
	${SHR_SOURCES}
)

set_target_properties (
	${DPSHAREDLIB}
	PROPERTIES
	OUTPUT_NAME drawpile
	VERSION 0.4.0
	SOVERSION 0.4.0
	PROJECT_LABEL drawpile-library
)

if ( SHARED )
	strip_lib ( ${DPSHAREDLIB} )
	if ( WIN32 )
		install ( TARGETS ${DPSHAREDLIB} RUNTIME DESTINATION . )
	else ( WIN32 )
		install ( TARGETS ${DPSHAREDLIB} LIBRARY DESTINATION lib )
	endif ( WIN32 )
endif ( SHARED )

if ( OPENSSL )
	find_package ( OpenSSL QUIET )
	if ( OPENSSL_FOUND )
		set ( HAVE_OPENSSL ON )
		message ( STATUS "OpenSSL libraries found!" )
		target_link_libraries ( ${DPSHAREDLIB} ${OPENSSL_LIBRARIES} )
	endif ( OPENSSL_FOUND )
endif ( OPENSSL )

configure_file ( config.h.cmake config.h )
