find_package(Qt5Gui REQUIRED)

set (
	SOURCES
	main.cpp
	smartserver.cpp
	thickserverclient.cpp
	thicksession.cpp
	)

add_executable(${SMARTSRVNAME} ${SOURCES})
target_link_libraries( ${SMARTSRVNAME}  ${DPSHAREDLIB} ${DPCLIENTLIB} Qt5::Network Qt5::Gui )

#if(TESTS)
	#add_subdirectory(tests)
#endif(TESTS)

