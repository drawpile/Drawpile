# src/
add_subdirectory ( shared/ )

if ( CLIENT )
	add_subdirectory ( client/ )
endif ( CLIENT )

if ( SERVER )
	add_subdirectory ( server/ )
endif ( SERVER )

