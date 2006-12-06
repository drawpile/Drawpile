project (DrawPile)

configure_file ( config.h.cmake config.h)

#set (PROJECT_BINARY_DIR ../)
#set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
#set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

#include ( CheckIncludeFiles )

include (CheckSymbolExists)

if (WIN32)
	check_symbol_exists ( INVALID_SOCKET "winsock2.h" HAVE_INVALID_SOCKET )
	check_symbol_exists ( SOCKET_ERROR "winsock2.h" HAVE_SOCKET_ERROR )
else (WIN32)
	check_symbol_exists ( INVALID_SOCKET "sys/socket.h" HAVE_INVALID_SOCKET )
	check_symbol_exists ( SOCKET_ERROR "sys/socket.h" HAVE_SOCKET_ERROR )
endif (WIN32)


