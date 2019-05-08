if ( APPLE )
	set ( CLIENTNAME Drawpile )
else ()
	set ( CLIENTNAME drawpile )
endif ()

set ( DPSHAREDLIB "drawpilenet" )
set ( DPCLIENTLIB "drawpileclient" )

set ( SRVNAME "${PROJECT_NAME}-srv" )
set ( SMARTSRVNAME "${PROJECT_NAME}-smartsrv" )
set ( SRVLIB "lib${SRVNAME}" )

