# src/
add_subdirectory ( shared/ )

if ( SERVER )
	add_subdirectory ( server/ )
endif ( NOT NOSERVER )

if ( CLIENT )
	add_subdirectory ( client/ )
endif ( NOT NOCLIENT )

if ( UNITTEST )
	add_subdirectory ( unittest/ )
endif ( UNITTEST )
