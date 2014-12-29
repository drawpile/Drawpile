# src/CMakeLists.txt

add_subdirectory ( shared )

if ( CLIENT )
        add_subdirectory ( client )
endif ()

if ( SERVER )
        add_subdirectory ( server )
endif ()

if ( TOOLS )
        add_subdirectory ( tools )
endif ()

