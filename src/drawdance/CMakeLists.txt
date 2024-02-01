# SPDX-License-Identifier: MIT

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

include(DrawdanceCompilerOptions)
include(DrawdanceHelpers)
include(DrawdanceOptions)
include(DrawdanceDependencies)

if(TESTS)
    add_subdirectory(libtest)
endif()

add_subdirectory(bundled)
add_subdirectory(generators)
add_subdirectory(libcommon)
add_subdirectory(libmsg)
add_subdirectory(libengine)

add_library(drawdance INTERFACE)
set(drawdance_libraries dpengine dpengine_rust dpmsg dpmsg_rust dpcommon cmake-config)

# Deal with circular link dependencies using link groups. That's only needed on
# the GNU linker, which only resolves symbols linearly, MSVC and Clang don't.
if(${CMAKE_VERSION} VERSION_LESS "3.24.0")
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		message(WARNING
			"CMake version ${CMAKE_VERSION} is older than 3.24.0, link groups "
			"are unavailable. Will try to make it work anyway, knock on wood.")
		# This ends up grouping too many libraries, but otherwise seems to work.
		# Better than refusing operation on old cmake versions I guess.
		target_link_libraries(drawdance INTERFACE
			-Wl,--start-group ${drawdance_libraries} -Wl,--end-group
		)
	else()
		message(STATUS "Assuming rescan is not needed on this linker")
        target_link_libraries(drawdance INTERFACE ${drawdance_libraries})
	endif()
else()
    if(CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED OR CMAKE_CXX_LINK_GROUP_USING_RESCAN_SUPPORTED)
        list(JOIN drawdance_libraries "," drawdance_libraries_rescan)
		target_link_libraries(drawdance INTERFACE
			"$<LINK_GROUP:RESCAN,${drawdance_libraries_rescan}>"
		)
	else()
		message(STATUS "Rescan is not supported, assuming it's not needed")
        target_link_libraries(drawdance INTERFACE ${drawdance_libraries})
	endif()
endif()
