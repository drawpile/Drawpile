# src/shared/

if (WIN32)
	link_libraries (ws2_32) # winsock2
endif (WIN32)

add_library (
	libdrawpile
	${LIBTYPE}
	protocol.cpp
	protocol.helper.cpp
	SHA1.cpp
)

set_target_properties (
	libdrawpile
	PROPERTIES
	OUTPUT_NAME drawpile
	VERSION 0.4.0
	SOVERSION 0.4.0
	PROJECT_LABEL drawpile-library
)

if ( SHARED )
	install ( TARGETS libdrawpile LIBRARY DESTINATION lib )
endif ( SHARED )
