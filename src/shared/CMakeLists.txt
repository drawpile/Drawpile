# src/shared/

set (
	Shared_Headers
	SHA1.h
	protocol.h
	protocol.admin.h
	protocol.defaults.h
	protocol.errors.h
	protocol.tools.h
	protocol.flags.h
	protocol.types.h
	protocol.helper.h
	memstack.h
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
	dpshared
	${Shared_Sources}
	${Shared_Headers}
)

