# src/shared/

set (
	Shared_Headers
	SHA1.h
	protocol.h
	protocol.defaults.h
	protocol.errors.h
	protocol.flags.h
	protocol.types.h
	protocol.helper.h
	memstack.h
	protocol.tools.h
	datatypes.h
)

set (
	Shared_Sources
	protocol.cpp
	protocol.helper.cpp
	SHA1.cpp
)

if (WIN32)
	link_libraries (ws2_32) # winsock2
endif (WIN32)

add_library (
	libdrawpile
	${LIBTYPE}
	${Shared_Sources}
	${Shared_Headers}
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
