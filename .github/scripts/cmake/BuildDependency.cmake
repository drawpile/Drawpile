include(ProcessorCount)
ProcessorCount(NPROCS)

# ExternalProject, except useful!
function(build_dependency name version build_type)
	set(oneValueArgs URL)
	set(multiValueArgs VERSIONS ALL_PLATFORMS UNIX WIN32)
	cmake_parse_arguments(PARSE_ARGV 3 ARG "" "${oneValueArgs}" "${multiValueArgs}")

	if(ARG_ALL_PLATFORMS)
		set(BUILD_ARGS ${ARG_ALL_PLATFORMS})
	elseif(WIN32 AND ARG_WIN32)
		set(BUILD_ARGS ${ARG_WIN32})
	elseif(UNIX AND ARG_UNIX)
		set(BUILD_ARGS ${ARG_UNIX})
	else()
		message(FATAL_ERROR "No instruction to build ${name} on this platform")
	endif()

	list(POP_FRONT BUILD_ARGS generator)
	string(TOLOWER "${generator}" generator)
	if(generator STREQUAL "automake" AND EXISTS "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/${name}.pc")
		message("${name} already exists in ${CMAKE_INSTALL_PREFIX}; skipping")
		return()
	endif()

	list(FIND ARG_VERSIONS "${version}" hash_index)
	if(hash_index EQUAL -1)
		set(url_hash "")
	else()
		math(EXPR hash_index "${hash_index} + 1")
		list(GET ARG_VERSIONS ${hash_index} url_hash)
	endif()

	string(REPLACE "@version@" "${version}" url "${ARG_URL}")
	set(source_dir "${CMAKE_BINARY_DIR}/${name}-${version}")
	if(EXISTS "${source_dir}")
		message(STATUS "Reusing existing source directory")
	else()
		_download("${url}" "${url_hash}")
	endif()
	message(STATUS "Installing ${source_dir}")

	if(generator STREQUAL "automake")
		_build_automake("${build_type}" "${source_dir}" ${BUILD_ARGS})
	elseif(generator STREQUAL "cmake")
		_build_cmake("${build_type}" "${source_dir}" ${BUILD_ARGS})
	elseif(WIN32 AND generator STREQUAL "msbuild")
		_build_msbuild("${build_type}" "${source_dir}" ${BUILD_ARGS})
	else()
		message(FATAL_ERROR "Unknown build kind '${generator}'")
	endif()

	file(REMOVE_RECURSE "${source_dir}")
endfunction()

function(_build_automake build_type source_dir)
	_parse_flags("${build_type}" configure_flags ${ARGN})

	if(NPROCS EQUAL 0)
		set(make_flags "")
	else()
		set(make_flags "-j${NPROCS}")
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E env "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"${source_dir}/configure" --prefix "${CMAKE_INSTALL_PREFIX}" ${configure_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E env "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			make install ${make_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
endfunction()

function(_build_cmake build_type source_dir)
	_parse_flags("${build_type}" configure_flags ${ARGN})

	set(install_flags "--config;Release")
	set(make_flags "${install_flags}")
	if(NOT NPROCS EQUAL 0)
		set(make_flags "${make_flags};--parallel;${NPROCS}")
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" "${source_dir}"
			"-DCMAKE_BUILD_TYPE=Release"
			"-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
			"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
			${configure_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --build "${source_dir}" ${make_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --install "${source_dir}" ${install_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
endfunction()

function(_build_msbuild build_type source_dir)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "SOLUTION;SHARED;STATIC" "INCLUDES;RM")

	if(NOT CMAKE_VS_MSBUILD_COMMAND)
		message(FATAL_ERROR "CMake has no MSBuild.exe")
	endif()

	foreach(config IN ITEMS ${ARG_SHARED} ${ARG_STATIC})
		execute_process(
			COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" -m
				"-p:Configuration=${config};Platform=x64;OutDir=${CMAKE_INSTALL_PREFIX}"
				"${source_dir}/${ARG_SOLUTION}"
			COMMAND_ECHO STDOUT
			WORKING_DIRECTORY "${source_dir}"
			COMMAND_ERROR_IS_FATAL ANY
		)
	endforeach()

	list(TRANSFORM ARG_RM PREPEND "${CMAKE_INSTALL_PREFIX}/")

	if(ARG_RM)
		file(REMOVE ${ARG_RM})
	endif()

	list(TRANSFORM ARG_INCLUDES PREPEND "${source_dir}/")

	if(ARG_INCLUDES)
		file(COPY ${ARG_INCLUDES}
			DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
		)
	endif()
endfunction()

function(_download url hash)
	get_filename_component(filename "${url}" NAME)
	message(STATUS "Downloading ${url}...")
	foreach(retry RANGE 0 3)
		file(DOWNLOAD "${url}" "${filename}"
			INACTIVITY_TIMEOUT 30
			STATUS result
		)
		list(GET result 0 status)
		if(status EQUAL 0)
			break()
		endif()
	endforeach()

	# Gotta check hash separately since if there is a temporary network failure
	# `file(DOWNLOAD EXPECTED_HASH)` will raise a fatal error
	if(status EQUAL 0)
		if(hash)
			string(REPLACE "=" ";" hash "${hash}")
			list(GET hash 0 algo)
			list(GET hash 1 expected)
			string(TOLOWER "${expected}" expected)
			file(${algo} "${filename}" actual)
			if(expected STREQUAL "" OR actual STREQUAL "")
				status(FATAL_ERROR "Hash computation failed!")
			elseif(NOT expected STREQUAL actual)
				status(FATAL_ERROR "Hash check failed! ${expected} != ${actual}")
			endif()
		else()
			status(WARNING "No hash available")
		endif()
	else()
		list(GET result 1 result)
		message(FATAL_ERROR "Failed: ${result}")
	endif()

	message(STATUS "Extracting ${filename}...")
	file(ARCHIVE_EXTRACT INPUT "${filename}")
	file(REMOVE "${filename}")
endfunction()

function(_parse_flags build_type out_var)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "ALL;DEBUG;RELEASE")

	if(build_type STREQUAL "debug" AND ARG_DEBUG)
		set(${out_var} "${ARG_ALL};${ARG_DEBUG}" PARENT_SCOPE)
	elseif(build_type STREQUAL "release" AND ARG_RELEASE)
		set(${out_var} "${ARG_ALL};${ARG_RELEASE}" PARENT_SCOPE)
	else()
		set(${out_var} "${ARG_ALL}" PARENT_SCOPE)
	endif()
endfunction()
