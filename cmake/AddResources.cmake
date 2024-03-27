#[[
Adds files and directories to a target as installable resources.
#]]
function(add_resources target)
	set(multiValueArgs FILES DIRS)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "BUILD_TIME_DESTINATION" "${multiValueArgs}")

	if(NOT target)
		message(FATAL_ERROR "missing required target")
	endif()

	foreach(dir IN LISTS ARG_DIRS)
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

		if(ARG_BUILD_TIME_DESTINATION)
			add_custom_command(
				TARGET ${target} PRE_LINK
				COMMAND "${CMAKE_COMMAND}" -E make_directory
					"${ARG_BUILD_TIME_DESTINATION}"
				COMMAND "${CMAKE_COMMAND}" -E create_symlink
					"${CMAKE_CURRENT_SOURCE_DIR}/assets/${dir}"
					"${ARG_BUILD_TIME_DESTINATION}/${dir}"
				COMMAND_EXPAND_LISTS
				COMMENT "Creating symlink to resource ${dir} in build directory"
				VERBATIM
			)
		endif()

		if(ANDROID OR APPLE OR EMSCRIPTEN)
			# Nothing to do, assets will already be installed by
			# MACOSX_PACKAGE_LOCATION on macOS, by Gradle on Android and through
			# an asset bundle in the browser.
		else()
			# No trailing slash is required or else it will strip the last path
			# of the directory
			install(DIRECTORY "assets/${dir}"
				DESTINATION "${INSTALL_APPDATADIR}"
			)
		endif()
	endforeach()

	foreach(file IN LISTS ARG_FILES)
		target_sources(${target} PRIVATE "assets/${file}")
		get_filename_component(file_dir "${file}" DIRECTORY)
		set_source_files_properties("assets/${file}"
			PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${file_dir}"
		)

		if(ARG_BUILD_TIME_DESTINATION)
			add_custom_command(
				TARGET ${target} PRE_LINK
				COMMAND "${CMAKE_COMMAND}" -E make_directory
					"${ARG_BUILD_TIME_DESTINATION}/${file_dir}"
				COMMAND "${CMAKE_COMMAND}" -E create_symlink
					"${CMAKE_CURRENT_SOURCE_DIR}/assets/${file}"
					"${ARG_BUILD_TIME_DESTINATION}/${file}"
				COMMAND_EXPAND_LISTS
				COMMENT "Creating symlink to resource ${file} in build directory"
				VERBATIM
			)
		endif()

		if(ANDROID OR APPLE OR EMSCRIPTEN)
			# Nothing to do, assets will already be installed by
			# MACOSX_PACKAGE_LOCATION on macOS, by Gradle on Android and through
			# an asset bundle in the browser.
		else()
			install(FILES "assets/${file}"
				DESTINATION "${INSTALL_APPDATADIR}/${file_dir}"
			)
		endif()
	endforeach()
endfunction()
