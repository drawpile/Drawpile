find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)

set (
	STANDALONE_SOURCES
	main.cpp
	)

add_executable("drawpile-thicksrv" ${STANDALONE_SOURCES})
target_link_libraries("drawpile-thicksrv" thicksrvlib )

#if(TESTS)
	#add_subdirectory(tests)
#endif(TESTS)

