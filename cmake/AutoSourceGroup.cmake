#[[ This module defines settings and functions for generating source groups. #]]

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP "Generated Files")
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER "Generated Targets")

#[[
Generates source groups used by Xcode and Visual Studio projects for all targets
in the current source directory.
This must be run in every single directory separately; see
https://gitlab.kitware.com/cmake/cmake/-/issues/18856.
#]]
function(directory_auto_source_groups)
	get_property(targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
	target_auto_source_group(${targets})
endfunction()

#[[
Generates idiomatic source groups used by Xcode and Visual Studio projects.
This must be run in every single directory separately; see
https://gitlab.kitware.com/cmake/cmake/-/issues/18856.
#]]
function(target_auto_source_group)
	foreach(target IN LISTS ARGN)
		get_target_property(sources ${target} SOURCES)
		get_target_property(resources ${target} RESOURCE)

		get_target_property(tree_base ${target} DP_AUTO_SOURCE_TREE_BASE)
		if(NOT tree_base)
			set(tree_base "${CMAKE_CURRENT_SOURCE_DIR}")
		elseif(NOT IS_ABSOLUTE "${tree_base}")
			set(tree_base "${CMAKE_CURRENT_SOURCE_DIR}/${tree_base}")
		endif()

		foreach(filename IN LISTS sources)
			if(filename MATCHES "(\\.[Hh]([Pp][Pp])?|include[\\/][^.]*)$")
				set(prefix "Header Files")
			elseif(filename MATCHES "\\.[Cc]([Pp][Pp])?$")
				set(prefix "Source Files")
			else()
				set(prefix "Resources")
			endif()

			string(FIND "${filename}" "${CMAKE_CURRENT_BINARY_DIR}" bin_dir_pos)
			if(bin_dir_pos EQUAL 0)
				source_group("${prefix}" FILES ${filename})
			else()
				source_group(TREE "${tree_base}" PREFIX "${prefix}" FILES ${filename})
			endif()
		endforeach()
	endforeach()
endfunction()
