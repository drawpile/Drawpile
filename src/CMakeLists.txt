# src/
add_subdirectory ( shared/ )

if ( SERVER )
	add_subdirectory ( server/ )
endif ( SERVER )

if ( CLIENT )
	add_subdirectory ( client/ )
endif ( CLIENT )
