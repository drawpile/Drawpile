#[[
Configures a file at build time using the current Git revision.
#]]
function(git_version_configure_file in_file out_file)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "TARGET_NAME;VAR;BUILD_LABEL" "")

	if(NOT ARG_TARGET_NAME)
		set(ARG_TARGET_NAME git_version)
	endif()

	if(NOT ARG_VAR)
		message(FATAL_ERROR "Missing required VAR")
	endif()

	if(NOT IS_ABSOLUTE "${in_file}")
		set(in_file "${CMAKE_CURRENT_SOURCE_DIR}/${in_file}")
	endif()

	if(NOT IS_ABSOLUTE "${out_file}")
		set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${out_file}")
	endif()

	get_filename_component(out_filename "${out_file}" NAME)

	if(BUILD_VERSION)
		message(STATUS "Generating ${out_filename} with BUILD_VERSION")
		_git_make_version(${ARG_VAR} "${BUILD_VERSION}" "${ARG_BUILD_LABEL}")
		configure_file("${in_file}" "${out_file}")
	else()
		find_package(Git)
		if(Git_FOUND)
			add_custom_target("${ARG_TARGET_NAME}" ALL
				DEPENDS "${in_file}"
				BYPRODUCTS "${out_file}"
				COMMENT "Generating ${out_filename} with Git version"
				COMMAND "${CMAKE_COMMAND}"
					"-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
					"-DGIT_VERSION_IN_FILE=${in_file}"
					"-DGIT_VERSION_OUT_FILE=${out_file}"
					"-DGIT_VERSION_BUILD_LABEL=${ARG_BUILD_LABEL}"
					"-DGIT_VERSION_VAR=${ARG_VAR}"
					"-DGIT_VERSION_FALLBACK=${PROJECT_VERSION}"
					-P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			)
		else()
			message(STATUS "Generating ${out_filename} with static version")
			_git_make_version(${ARG_VAR} "${PROJECT_VERSION}" "${ARG_BUILD_LABEL}")
			configure_file("${in_file}" "${out_file}")
		endif()
	endif()
endfunction()

#[[
Gets the current Git revision.
#]]
function(git_version_describe out_var)
	if(BUILD_VERSION)
		message(STATUS "BUILD_VERSION set, using it instead of git")
		set(${out_var} "${BUILD_VERSION}" PARENT_SCOPE)
	else()
		if(NOT GIT_EXECUTABLE)
			find_package(Git)
		endif()

		if(GIT_EXECUTABLE)
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" describe --dirty
				OUTPUT_VARIABLE version
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
			)
		else()
			message(STATUS "Git not found; falling back to static versioning")
			set(version "")
		endif()

		set(${out_var} "${version}" PARENT_SCOPE)
	endif()
endfunction()

function(_git_make_version out_var version label)
	if(label)
		set(${out_var} "${version}+${label}" PARENT_SCOPE)
	else()
		set(${out_var} "${version}" PARENT_SCOPE)
	endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
	git_version_describe(version)
	if(NOT version)
		message(WARNING "Couldn't determine git version; falling back to static versioning")
		set(version "${GIT_VERSION_FALLBACK}")
	endif()
	_git_make_version(${GIT_VERSION_VAR} "${version}" "${GIT_VERSION_BUILD_LABEL}")
	configure_file("${GIT_VERSION_IN_FILE}" "${GIT_VERSION_OUT_FILE}")
endif()
