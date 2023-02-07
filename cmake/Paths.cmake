if(APPLE)
	set(CLIENTNAME Drawpile)
else()
	set(CLIENTNAME drawpile)
endif()

set(SRVNAME "${PROJECT_NAME}-srv")
set(SMARTSRVNAME "${PROJECT_NAME}-smartsrv")
set(SRVLIB "lib${SRVNAME}")

# binary and library output
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
