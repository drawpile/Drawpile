# src/CMakeLists.txt

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
#find_package(KF5DNSSD NO_MODULE) # disabled pending built-in server rewrite

if(KF5DNSSD_FOUND)
	add_definitions(-DHAVE_DNSSD)
	# KF5DNSSD 8.84.0 moves a bunch of headers to a different place.
	if("${KF5DNSSD_VERSION}" VERSION_LESS "5.84.0")
		add_definitions(-DHAVE_DNSSD_BEFORE_5_84_0)
	endif()
endif()

add_subdirectory(libshared)
add_subdirectory(libserver)
add_subdirectory(rustpile)

if(CLIENT)
	add_subdirectory(dpmypaint/bundled/libmypaint)
	add_subdirectory(libclient)
endif()

if(CLIENT)
	add_subdirectory(desktop)
endif()

if(SERVER)
	add_subdirectory (thinsrv)
endif()

