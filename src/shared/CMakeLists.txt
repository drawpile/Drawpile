set (
	Shared_Headers
	protocol.h
	sockets.h
	SHA1.h
	protocol.admin.h
	protocol.defaults.h
	protocol.errors.h
	protocol.tools.h
	protocol.flags.h
	protocol.types.h
)

set (
	Shared_Sources
	protocol.cpp
	sockets.cpp
	SHA1.cpp
)

if (WIN32)
	link_libraries (ws2_32)
endif (WIN32)

add_library (
	dpshared
	${Shared_Sources}
	${Shared_Headers}
)

