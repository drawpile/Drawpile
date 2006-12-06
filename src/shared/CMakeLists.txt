project (DrawPile)

set (SHARED_HEADERS protocol.h sockets.h SHA1.h protocol.admin.h protocol.defaults.h protocol.errors.h protocol.tools.h protocol.flags.h protocol.types.h)

IF (WIN32)
	link_libraries (ws2_32)
ENDIF (WIN32)

add_library (dpshared protocol.cpp sockets.cpp SHA1.cpp ${SHARED_HEADERS})

