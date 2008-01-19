# src/shared/

set (
	SHR_SOURCES
	protocol.cpp
	protocol.helper.cpp
	SHA1.cpp
)

if ( RELEASE )
	generate_final ( SHR_SOURCES ${SHR_SOURCES} )
endif ( )

add_library ( ${DPSHAREDLIB} ${SHR_SOURCES} )

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
	else ( )
		install ( TARGETS ${DPSHAREDLIB} LIBRARY DESTINATION lib )
	endif ( )
endif ( )

set ( HAVE_OPENSSL false )
if ( OPENSSL )
	find_package ( OpenSSL QUIET )
	if ( OPENSSL_FOUND )
		set ( HAVE_OPENSSL true )
		message ( STATUS "OpenSSL libraries found!" )
		target_link_libraries ( ${DPSHAREDLIB} ${OPENSSL_LIBRARIES} )
	endif ( )
endif ( )

configure_file ( config.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/config.h )
