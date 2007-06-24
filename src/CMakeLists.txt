# src/
add_subdirectory ( shared/ )

if ( CLIENT )
	add_subdirectory ( client/ )
endif ( CLIENT )

if ( SERVERLIB )
	add_subdirectory ( server/ )
endif ( SERVERLIB )

if ( TRAYSERVER )
	add_subdirectory ( tray-server/ )
endif ( TRAYSERVER )

if ( UNITTEST )
	add_subdirectory ( unittest/ )
endif ( UNITTEST )
