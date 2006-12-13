project ( DrawPile CXX C )

cmake_minimum_required( VERSION 2.4.0 )

### config options ###

option ( NOCLIENT "Don't compile client" OFF )
option ( NOSERVER "Don't compile server" OFF )
option ( UNITTEST "Compile unittests" OFF )
option ( NOEPOLL "Disable epoll(4) support" OFF )
option ( NOPSELECT "Disable pselect() support" OFF )
option ( NOKQUEUE "Disable kqueue() support" OFF )


### binary and library output ###

set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin )
set ( LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib )

### config.h ###

configure_file ( config.h.cmake config.h )


# check portability stuff

include ( CheckSymbolExists )
include ( CheckFunctionExists )
include ( CheckIncludeFiles )
include ( CheckVariableExists )

check_include_files ( stdint.h HAVE_STDINT_H )

# EPOLL
if ( NOT NOEPOLL )
	check_include_files ( sys/epoll.h EV_EPOLL )
endif ( NOT NOEPOLL )

# KQUEUE
if ( NOT NOKQUEUE )
	check_function_exists ( kqueue EV_KQUEUE )
endif ( NOT NOKQUEUE )

# PSELECT
if ( NOT NOPSELECT )
	check_function_exists ( pselect EV_PSELECT )
endif ( NOT NOPSELECT )

# fall back to select
set ( EV_SELECT 1 )
check_variable_exists ( EV_SELECT EV_SELECT )

check_symbol_exists ( FD_COPY "sys/select.h;unistd.h" HAVE_SELECT_COPY )

if ( WIN32 )
	check_include_files ( winsock2.h USE_WINDOWS_SOCKET_API )
	check_symbol_exists ( WSAEWOULDBLOCK "winsock2.h" HAVE_WSA )
endif ( WIN32 )

### tests ###

include ( TestForSTDNamespace )

#! needs C in project ()
include ( TestBigEndian )
test_big_endian ( IS_BIG_ENDIAN )
check_variable_exists ( IS_BIG_ENDIAN IS_BIG_ENDIAN )


### subdirs ###

add_subdirectory( src/ )
