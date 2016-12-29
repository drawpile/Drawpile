# src/server/CMakeLists.txt

find_package( Qt5Network REQUIRED)
find_package( Qt5Sql REQUIRED)
find_package( KF5Archive REQUIRED NO_MODULE )

set (
	SOURCES
	main.cpp
	multiserver.cpp
	sslserver.cpp
	database.cpp
	headless/headless.cpp
	)

# Unix specific features
if ( UNIX )
	set ( SOURCES ${SOURCES} headless/unixsignals.cpp )
endif ( UNIX )

# Select init system integration backend
if ( INITSYS STREQUAL "systemd" AND SYSTEMD_FOUND )
	set ( SOURCES ${SOURCES} initsys_systemd.cpp )
	set ( INITSYS_LIB ${SYSTEMD_LIBRARIES} )
else ()
	set ( SOURCES ${SOURCES} initsys_dummy.cpp )
endif ()

add_executable( ${SRVNAME} ${SOURCES} )
target_link_libraries( ${SRVNAME}  ${DPSHAREDLIB} Qt5::Network Qt5::Sql ${INITSYS_LIB} ${MHD_LIBRARIES} )

if ( UNIX AND NOT APPLE )
	install ( TARGETS ${SRVNAME} DESTINATION bin )
endif ()
