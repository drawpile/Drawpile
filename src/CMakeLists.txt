add_subdirectory(libshared)
add_subdirectory(libserver)

if(CLIENT)
	add_subdirectory(libclient)
	add_subdirectory(desktop)
endif()

if(SERVER)
	add_subdirectory(thinsrv)
endif()
