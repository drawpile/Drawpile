#[[
Adds files and directories to a target as installable resources.
#]]
function(add_resources target)
	set(multiValueArgs FILES DIRS)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "${multiValueArgs}")

	if(NOT target)
		message(FATAL_ERROR "missing required target")
		return()
	endif()

	foreach(dir IN ITEMS palettes sounds theme)
		file(GLOB_RECURSE resources
			RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
			LIST_DIRECTORIES FALSE
			"assets/${dir}/*"
		)
		foreach(resource IN LISTS resources)
			target_sources(${target} PRIVATE "assets/${resource}")
			get_filename_component(resource_dir "${resource}" DIRECTORY)
			set_source_files_properties("assets/${resource}"
				PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${resource_dir}"
			)
		endforeach()
		# Assets will already be installed by MACOSX_PACKAGE_LOCATION on macOS
		# and by symlinking on Android
		if(NOT APPLE AND NOT ANDROID)
			# No trailing slash is required or else it will strip the last path
			# of the directory
			install(DIRECTORY "assets/${dir}"
				DESTINATION "${INSTALL_APPDATADIR}"
				COMPONENT "${target}"
			)
		endif()
	endforeach()

	foreach(file IN LISTS ARG_FILES)
		target_sources(${target} PRIVATE "assets/${file}")
		set_property(TARGET ${target} APPEND PROPERTY RESOURCE "assets/${file}")
	endforeach()
endfunction()
