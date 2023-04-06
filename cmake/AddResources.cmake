#[[
Adds files and directories to a target as installable resources.
#]]
function(add_resources target)
	set(multiValueArgs FILES DIRS)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "${multiValueArgs}")

	if(NOT target)
		message(FATAL_ERROR "missing required target")
	endif()

	# For Android, the assets must be set up in advance in one directory because
	# Qt only accepts one directory for all extra files and puts them in the APK
	# at build time
	if(ANDROID)
		get_target_property(android_dir ${target} QT_ANDROID_PACKAGE_SOURCE_DIR)
		if(NOT android_dir)
			message(FATAL_ERROR "Missing QT_ANDROID_PACKAGE_SOURCE_DIR for AddResources")
		endif()
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

		if(ANDROID)
			file(CREATE_LINK
				"${CMAKE_CURRENT_SOURCE_DIR}/assets/${dir}"
				"${android_dir}/assets/${dir}"
				SYMBOLIC
			)
		# Assets will already be installed by MACOSX_PACKAGE_LOCATION on macOS
		elseif(NOT APPLE)
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

		# As of Qt 6.4, its Android support ignores RESOURCE type files
		if(ANDROID)
			file(CREATE_LINK
				"${CMAKE_CURRENT_SOURCE_DIR}/assets/${file}"
				"${android_dir}/assets/${file}"
				SYMBOLIC
			)
		endif()
	endforeach()
endfunction()
