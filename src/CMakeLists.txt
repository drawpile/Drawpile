# src/CMakeLists.txt

add_subdirectory ( shared )

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(KF5DNSSD NO_MODULE)
find_package(GIF)
find_package(Miniupnpc)

if ( CLIENT )
        add_subdirectory ( client )
        add_subdirectory ( desktop )
endif ()

if ( SERVER )
        add_subdirectory ( server )
endif ()

if ( SMARTSRV )
	add_subdirectory ( smartsrv )
endif ()

if ( TOOLS )
        add_subdirectory ( tools )
endif ()

