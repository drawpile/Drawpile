# src/
add_subdirectory ( shared/ )

if ( NOT NOSERVER )
	add_subdirectory ( server/ )
endif ( NOT NOSERVER )

if ( NOT NOCLIENT )
	add_subdirectory ( client/ )
endif ( NOT NOCLIENT )

if ( UNITTEST )
	add_subdirectory ( unittest/ )
endif ( UNITTEST )
