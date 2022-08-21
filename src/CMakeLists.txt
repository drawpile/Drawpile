add_subdirectory(libshared)
add_subdirectory(libserver)
add_subdirectory(rustpile)

if(CLIENT)
	add_subdirectory(dpmypaint/bundled/libmypaint)
	add_subdirectory(libclient)
	add_subdirectory(desktop)
endif()

if(SERVER)
	add_subdirectory(thinsrv)
endif()
