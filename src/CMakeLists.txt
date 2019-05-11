# src/CMakeLists.txt

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(KF5DNSSD NO_MODULE)
find_package(GIF)
find_package(Miniupnpc)

if(LIBMINIUPNPC_FOUND)
	add_definitions(-DHAVE_UPNP)
endif()

if(KF5DNSSD_FOUND)
	add_definitions(-DHAVE_DNSSD)
endif()

add_subdirectory ( shared )

if ( CLIENT )
	add_subdirectory ( client )
	add_subdirectory ( thicksrv )
	add_subdirectory ( desktop )
endif ()

if ( SERVER )
        add_subdirectory ( server )
endif ()

if ( TOOLS )
        add_subdirectory ( tools )
endif ()

