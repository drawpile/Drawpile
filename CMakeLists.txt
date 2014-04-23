project ( drawpile CXX C )

cmake_minimum_required( VERSION 2.8.9 )
set ( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/config" )

### generic info
set ( WEBSITE "http://drawpile.sourceforge.net/" )
set ( DRAWPILE_VERSION "0.8.6" )

### protocol versions
# see doc/protocol.md for protocol version history
set ( DRAWPILE_PROTO_MAJOR_VERSION 10 )
set ( DRAWPILE_PROTO_MINOR_VERSION 2 )
set ( DRAWPILE_PROTO_DEFAULT_PORT 27750 )

###
include ( "config/Names.cmake" )

### options ###
option ( CLIENT "Compile client" ON )
option ( SERVER "Compile UI-less server" ON )
option ( INSTALL_DOC "Install documents" ON )

option ( DEBUG "Enable debugging and asserts" OFF )
option ( GENERIC "Optimize for generic CPU arch" OFF )

option ( RELEASE "Enable final all-in-one compilation." OFF )

# Set build type
if ( DEBUG )
	set ( CMAKE_BUILD_TYPE Debug )
else ( )
	set ( CMAKE_BUILD_TYPE Release )
endif ( )

message ( STATUS "Build type: ${CMAKE_BUILD_TYPE}" )

### Optimize ###
include ( "config/Optimize.cmake" )

# Include some nice macros
include ( "config/Macros.cmake" )

### binary and library output ###
set ( EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin" )
set ( LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib" )

find_package ( ZLIB REQUIRED )
find_package ( PkgConfig )

if ( PKGCONFIG_FOUND )
	pkg_check_modules ( SYSTEMD "libsystemd" )
endif ( PKGCONFIG_FOUND )


### Compiler-specific C++ 11 activation ###
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_LESS 4.7)
        # GCC < 4.7
        set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
    else ()
        # GCC >= 4.7 (set instead of append, because it needs to be first)
        set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # Clang++
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
else ()
    # Other C++ compilers
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif ()

### Output config.h ###
configure_file ( config/config.h.cmake "${CMAKE_BINARY_DIR}/config.h" )
add_definitions ( -DHAVE_CONFIG_H )
# Tell the compiler where to find config.h
include_directories ( "${CMAKE_BINARY_DIR}" )

# scan sub-directories
add_subdirectory( src )

if ( INSTALL_DOC )
	add_subdirectory( doc )
endif ( )

add_subdirectory( desktop )

if ( CMAKE_BUILD_TYPE STREQUAL Debug )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_DEBUG}")
elseif ( CMAKE_BUILD_TYPE STREQUAL Release )
	message ( STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif ( )

