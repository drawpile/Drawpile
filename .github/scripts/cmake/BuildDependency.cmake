include(ProcessorCount)
ProcessorCount(NPROCS)

# ExternalProject, except useful!
function(build_dependency name version build_type)
	set(oneValueArgs URL SOURCE_DIR)
	set(multiValueArgs VERSIONS PATCHES ALL_PLATFORMS UNIX WIN32)
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

	string(REGEX REPLACE "\\.[0-9]+$" "" version_major "${version}")
	string(CONFIGURE "${ARG_URL}" url @ONLY)
	if(ARG_SOURCE_DIR)
		string(CONFIGURE "${ARG_SOURCE_DIR}" source_dir @ONLY)
	else()
		set(source_dir "${name}-${version}")
	endif()
	set(source_dir "${CMAKE_BINARY_DIR}/${source_dir}")
	if(EXISTS "${source_dir}")
		message(STATUS "Reusing existing source directory")
	else()
		_download("${url}" "${url_hash}")
	endif()
	message(STATUS "Installing ${source_dir}")

	if(ARG_PATCHES)
		find_program(patch patch REQUIRED)
		set(apply_patch FALSE)
		foreach(item IN LISTS ARG_PATCHES)
			if(item MATCHES "^[0-9]")
				if(item VERSION_EQUAL version)
					set(apply_patch TRUE)
				else()
					set(apply_patch FALSE)
				endif()
			elseif(apply_patch)
				execute_process(
					COMMAND "${patch}" -p1
					INPUT_FILE "${CMAKE_CURRENT_LIST_DIR}/${item}"
					WORKING_DIRECTORY "${source_dir}"
					COMMAND_ECHO STDOUT
					COMMAND_ERROR_IS_FATAL ANY
				)
			endif()
		endforeach()
	endif()

	if(generator STREQUAL "qmake")
		_build_qmake("${build_type}" "${source_dir}" ${BUILD_ARGS})
	elseif(generator STREQUAL "qt_module")
		_build_qt_module("${build_type}" "${source_dir}" ${BUILD_ARGS})
	elseif(generator STREQUAL "automake")
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
	set(configure "${source_dir}/configure")
	_parse_flags("${build_type}" "${source_dir}" configure configure_flags ${ARGN})

	if(NPROCS EQUAL 0)
		set(make_flags "")
	else()
		set(make_flags "-j${NPROCS}")
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E env "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"${configure}" --prefix "${CMAKE_INSTALL_PREFIX}" ${configure_flags}
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
	set(configure "${CMAKE_COMMAND}" -S "${source_dir}" -B .)
	cmake_parse_arguments(PARSE_ARGV 2 "CMAKE_ARG" "NO_DEFAULT_FLAGS;NO_DEFAULT_BUILD_TYPE" "" "")
	_parse_flags("${build_type}" "${source_dir}" configure configure_flags ${CMAKE_ARG_UNPARSED_ARGUMENTS})

	if(build_type STREQUAL "debug")
		set(install_flags "--config;RelWithDebInfo")
	else()
		set(install_flags "--config;Release")
	endif()
	set(make_flags "${install_flags}")
	if(NOT NPROCS EQUAL 0)
		set(make_flags "${make_flags};--parallel;${NPROCS}")
	endif()

	if(CMAKE_ARG_NO_DEFAULT_FLAGS)
		set(default_flags "")
	else()
		set(default_flags
			"-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
			"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
		)
	endif()

	if(CMAKE_ARG_NO_DEFAULT_FLAGS OR CMAKE_ARG_NO_DEFAULT_BUILD_TYPE)
		set(build_flag "")
	elseif(build_type STREQUAL "debug")
		set(build_flag "-DCMAKE_BUILD_TYPE=RelWithDebInfo")
	else()
		set(build_flag "-DCMAKE_BUILD_TYPE=Release")
	endif()

	set(binary_dir "${source_dir}-build")
	file(MAKE_DIRECTORY "${binary_dir}")
	execute_process(
		COMMAND ${configure}
			${configure_flags}
			${build_flag}
			${default_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${binary_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --build . ${make_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${binary_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" --install . ${install_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${binary_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	file(REMOVE_RECURSE "${binary_dir}")
endfunction()

function(_build_msbuild build_type source_dir)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "SOLUTION;SHARED;STATIC" "INCLUDES;RM")

	if(NOT CMAKE_VS_MSBUILD_COMMAND)
		message(FATAL_ERROR "CMake has no MSBuild.exe")
	endif()

	execute_process(
		COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" -m
			"-p:Configuration=${ARG_SHARED};Platform=x64;OutDir=${CMAKE_INSTALL_PREFIX}/bin"
			"${source_dir}/${ARG_SOLUTION}"
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" -m
			"-p:Configuration=${ARG_STATIC};Platform=x64;OutDir=${CMAKE_INSTALL_PREFIX}/lib"
			"${source_dir}/${ARG_SOLUTION}"
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)

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

function(_build_qmake build_type source_dir)
	set(configure qmake)
	_parse_flags("${build_type}" "${source_dir}" configure configure_flags ${ARGN})

	if(configure STREQUAL qmake)
		find_program(configure qmake REQUIRED)
		list(APPEND configure "${source_dir}")
	endif()

	set(make_flags "")
	if(WIN32)
		find_program(make NAMES jom nmake REQUIRED)
	else()
		find_program(make make REQUIRED)
		if(NOT NPROCS EQUAL 0)
			set(make_flags -j${NPROCS})
		endif()
	endif()

	set(binary_dir "${source_dir}-build")
	file(MAKE_DIRECTORY "${binary_dir}")
	execute_process(
		COMMAND ${configure}
			"QMAKE_MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			${configure_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${binary_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	# Some install targets in Qt5 have broken dependency chains so
	# e.g. libqtpcre2.a will not be built before it tries to get installed
	# if we only use the install target
	execute_process(
		COMMAND "${make}" ${make_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${binary_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND "${make}" install ${make_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${binary_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	file(REMOVE_RECURSE "${binary_dir}")
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

function(_parse_flags build_type source_dir out_configurator out_flags)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "CONFIGURATOR;ALL;DEBUG;RELEASE")

	if(ARG_CONFIGURATOR)
		if(NOT IS_ABSOLUTE "${ARG_CONFIGURATOR}")
			set(ARG_CONFIGURATOR ${source_dir}/${ARG_CONFIGURATOR})
		endif()
		string(CONFIGURE "${ARG_CONFIGURATOR}" configurator @ONLY)
		set(${out_configurator} ${configurator} PARENT_SCOPE)
	endif()

	if(build_type STREQUAL "debug" AND ARG_DEBUG)
		set(${out_flags} "${ARG_DEBUG};${ARG_ALL}" PARENT_SCOPE)
	elseif(build_type STREQUAL "release" AND ARG_RELEASE)
		set(${out_flags} "${ARG_RELEASE};${ARG_ALL}" PARENT_SCOPE)
	else()
		set(${out_flags} "${ARG_ALL}" PARENT_SCOPE)
	endif()
endfunction()
