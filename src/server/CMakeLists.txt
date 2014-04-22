# src/server/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
find_package( Qt5Network )
include_directories( ${Qt5Network_INCLUDES} )
add_definitions(${Qt5Network_DEFINITIONS})

set (
	SOURCES
	dpserver.cpp
	multiserver.cpp
	)

if ( UNIX )
	set ( SOURCES ${SOURCES} unixsignals.cpp )
endif ( UNIX )

# Use local copy of Qt's command line parser for Qt <5.2
# Remove this when we no longer need to support old Qt.
if ( ${Qt5Network_VERSION_STRING} VERSION_LESS "5.2.0" )
	set ( SOURCES ${SOURCES} qcommandlineoption.cpp qcommandlineparser.cpp )
endif()

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} ${Qt5Network_LIBRARIES} )
qt5_use_modules( ${SRVNAME} Network )

if ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION . )
else ( WIN32 )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ( WIN32 )

