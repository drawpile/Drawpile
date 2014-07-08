if ( APPLE )
	set ( CLIENTNAME Drawpile )
else ()
	set ( CLIENTNAME drawpile )
endif ()

set ( DPSHAREDLIB "drawpilenet" )

set ( SRVNAME "${PROJECT_NAME}-srv" )
set ( SRVLIB "lib${SRVNAME}" )

