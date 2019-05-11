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
target_link_libraries( "thicksrvlib"  ${DPSHAREDLIB} ${DPCLIENTLIB} Qt5::Network Qt5::Gui )

set (
	STANDALONE_SOURCES
	main.cpp
	)

add_executable("drawpile-thicksrv" ${STANDALONE_SOURCES})

target_link_libraries( "drawpile-thicksrv" "thicksrvlib" )

#if(TESTS)
	#add_subdirectory(tests)
#endif(TESTS)

