find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)

set (
	SOURCES
	thickserver.cpp
	thickserverclient.cpp
	thicksession.cpp
	builtinserver.cpp
	builtinsession.cpp
	)

add_library( "thicksrvlib" STATIC ${SOURCES} )
target_link_libraries( "thicksrvlib"  dpserver dpclient Qt5::Network Qt5::Gui )

#if(TESTS)
	#add_subdirectory(tests)
#endif(TESTS)

