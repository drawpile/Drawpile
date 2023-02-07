#[[
Generates code for a basic FindXXX.cmake file.
#]]
macro(make_find_target name)
	set(multiValueArgs INCLUDE LIBRARY DEFINITIONS)
	cmake_parse_arguments(FIND_TARGET "" "" "${multiValueArgs}" ${ARGN})

	include(FindPackageHandleStandardArgs)

	# lib prefix is required because MSBuild solutions generate libraries with
	# that prefix even though it is not conventional for the platform
	if(WIN32)
		set(FIND_TARGET_LIBRARIES "")
		foreach(name IN LISTS FIND_TARGET_LIBRARY)
			list(APPEND FIND_TARGET_LIBRARIES "${name}" "lib${name}")
		endforeach()
	else()
		set(FIND_TARGET_LIBRARIES "${FIND_TARGET_LIBRARY}")
	endif()

	find_path(${name}_INCLUDE_DIR NAMES ${FIND_TARGET_INCLUDE})
	find_library(${name}_LIBRARY NAMES ${FIND_TARGET_LIBRARIES})
	mark_as_advanced(${name}_INCLUDE_DIR ${name}_LIBRARY)

	find_package_handle_standard_args(${name}
		REQUIRED_VARS ${name}_LIBRARY ${name}_INCLUDE_DIR
	)

	if(${name}_FOUND AND NOT TARGET ${name}::${name})
		add_library(${name}::${name} UNKNOWN IMPORTED)
		set_target_properties(${name}::${name} PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${${name}_INCLUDE_DIR}"
			IMPORTED_LOCATION "${${name}_LIBRARY}"
		)
		if(FIND_TARGET_DEFINITIONS)
			set_target_properties(${name}::${name} PROPERTIES
				INTERFACE_COMPILE_DEFINITIONS "${FIND_TARGET_DEFINITIONS}"
			)
		endif()
	endif()
endmacro()
