# src/shared/

set ( INTERNALNAME "lib${PROJECT_NAME}" )

if (WIN32)
	link_libraries (ws2_32) # winsock2
endif (WIN32)

set (
	SHR_SOURCES
	protocol.cpp
	protocol.helper.cpp
	SHA1.cpp
)

if ( RELEASE )
	generate_all_cpp ( "${SHR_SOURCES}" )
	set ( SHR_SOURCES auto_all.cpp )
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
