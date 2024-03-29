include(ProcessorCount)
ProcessorCount(NPROCS)

if(NOT CMAKE_INSTALL_PREFIX)
	message(FATAL_ERROR "`-DCMAKE_INSTALL_PREFIX` is required")
elseif(NOT IS_ABSOLUTE "${CMAKE_INSTALL_PREFIX}")
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}")
endif()

set(NEW_CMAKE_PREFIX_PATH "")
foreach(path IN LISTS CMAKE_PREFIX_PATH)
	if(IS_ABSOLUTE "${path}")
		list(APPEND NEW_CMAKE_PREFIX_PATH "${path}")
	else()
		list(APPEND NEW_CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/${path}")
	endif()
endforeach()
set(CMAKE_PREFIX_PATH ${NEW_CMAKE_PREFIX_PATH})
unset(NEW_CMAKE_PREFIX_PATH)

set(BUILD_TYPE "release" CACHE STRING
	"The type of build ('debug', 'debugnoasan', 'relwithdebinfo', or 'release')")
string(TOLOWER "${BUILD_TYPE}" BUILD_TYPE)
if(BUILD_TYPE STREQUAL "debug")
	set(USE_ASAN true)
	message(WARNING "This build type enables ASan for some dependencies!\n"
		"You may need to use `-DCMAKE_EXE_LINKER_FLAGS_INIT=-fsanitize=address`"
		" when linking to these libraries."
	)
elseif(BUILD_TYPE STREQUAL "debugnoasan")
	set(BUILD_TYPE "debug")
elseif(NOT BUILD_TYPE STREQUAL "release" AND NOT BUILD_TYPE STREQUAL "relwithdebinfo")
	message(FATAL_ERROR "Unknown build type '${BUILD_TYPE}'")
endif()

# It is probably the qt-cmake toolchain that contains useful information
# that we would enjoy having.
if(CMAKE_TOOLCHAIN_FILE)
	include(${CMAKE_TOOLCHAIN_FILE})
endif()

# ExternalProject, except useful!
function(build_dependency name version build_type)
	set(oneValueArgs URL SOURCE_DIR TARGET_BITS)
	set(multiValueArgs VERSIONS PATCHES ALL_PLATFORMS UNIX WIN32)
	cmake_parse_arguments(PARSE_ARGV 3 ARG "" "${oneValueArgs}" "${multiValueArgs}")

	if(ARG_TARGET_BITS STREQUAL "32")
		set(BUILD_TARGET_BITS 32)
	elseif(ARG_TARGET_BITS STREQUAL "64")
		set(BUILD_TARGET_BITS 64)
	else()
		message(FATAL_ERROR "Invalid TARGET_BITS '${ARG_TARGET_BITS}'")
	endif()

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
		# If the directory already exists, it was probably patched too!
		set(skip_patch true)
	else()
		_download("${url}" "${url_hash}")
	endif()
	message(STATUS "Installing ${source_dir}")

	if(ARG_PATCHES AND NOT skip_patch)
		find_program(patch patch REQUIRED)
		set(apply_patch FALSE)
		foreach(item IN LISTS ARG_PATCHES)
			if(item MATCHES "^[Aa][Ll][Ll]$")
				set(apply_patch TRUE)
			elseif(item MATCHES "^[0-9]")
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
	elseif(generator STREQUAL "automake")
		_build_automake("${build_type}" "${BUILD_TARGET_BITS}" "${source_dir}" ${BUILD_ARGS})
	elseif(generator STREQUAL "cmake")
		_build_cmake("${build_type}" "${BUILD_TARGET_BITS}" "${source_dir}" ${BUILD_ARGS})
	elseif(WIN32 AND generator STREQUAL "msbuild")
		_build_msbuild("${build_type}" "${BUILD_TARGET_BITS}" "${source_dir}" ${BUILD_ARGS})
	else()
		message(FATAL_ERROR "Unknown build kind '${generator}'")
	endif()

	if(NOT KEEP_SOURCE_DIRS)
		file(REMOVE_RECURSE "${source_dir}")
	endif()
endfunction()

function(_build_automake build_type target_bits source_dir)
	set(configure "${source_dir}/configure")
	cmake_parse_arguments(PARSE_ARGV 2 ARG "ASSIGN_PREFIX;BROKEN_INSTALL;NEEDS_VC_WIN_TARGET" "INSTALL_TARGET" "MAKE_FLAGS")
	_parse_flags("${build_type}" "${source_dir}" configure configure_flags env ${ARG_UNPARSED_ARGUMENTS})

	if(NPROCS EQUAL 0 OR WIN32)
		unset(make_flags)
	else()
		set(make_flags "-j${NPROCS}")
	endif()

	if(ARG_MAKE_FLAGS)
		list(APPEND make_flags ${ARG_MAKE_FLAGS})
	endif()

	list(APPEND env "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")

	# https://developer.android.com/ndk/guides/other_build_systems#autoconf
	if(CMAKE_ANDROID_NDK)
		get_android_env(android_env abi "${CMAKE_ANDROID_NDK}" "${CMAKE_ANDROID_ARCH_ABI}" "${ANDROID_PLATFORM}")
		list(APPEND env ${android_env})
		list(APPEND configure_flags --host "${abi}")
	endif()

	# OpenSSL has a terrible configurator that only accepts `--prefix=foo` and
	# does not bail out when it receives a bogus argument, so if you send
	# `--prefix foo` it will just install to its default prefix!
	if(ARG_ASSIGN_PREFIX)
		set(prefix "--prefix=${CMAKE_INSTALL_PREFIX}")
	else()
		set(prefix "--prefix" "${CMAKE_INSTALL_PREFIX}")
	endif()

	# OpenSSL somehow manages to find the 64bit compiler even when everything is
	# configured for 32bit, so it needs to be told explicitly what to build for.
	# This should be the last element in the configure_flags list!
	if(WIN32 AND ARG_NEEDS_VC_WIN_TARGET)
		if(target_bits EQUAL 32)
			list(APPEND configure_flags VC-WIN32)
		elseif(target_bits EQUAL 64)
			list(APPEND configure_flags VC-WIN64A)
		else()
			message(FATAL_ERROR "Invalid target_bits '${target_bits}' for ${ARG_SOLUTION}")
		endif()
	endif()

	if(ARG_INSTALL_TARGET)
		set(install ${ARG_INSTALL_TARGET})
	else()
		set(install install)
	endif()

	# On Windows, the only usable thing here is OpenSSL's pseudo-automake Perl
	# script. Shebangs don't work on Windows, so we have to run it explicitly.
	if(WIN32)
		set(make "nmake")
		execute_process(
			COMMAND "${CMAKE_COMMAND}" -E env ${env}
				perl "${configure}" ${prefix} ${configure_flags}
			COMMAND_ECHO STDOUT
			WORKING_DIRECTORY "${source_dir}"
			COMMAND_ERROR_IS_FATAL ANY
		)
	else()
		set(make "make")
		execute_process(
			COMMAND "${CMAKE_COMMAND}" -E env ${env}
				"${configure}" ${prefix} ${configure_flags}
			COMMAND_ECHO STDOUT
			WORKING_DIRECTORY "${source_dir}"
			COMMAND_ERROR_IS_FATAL ANY
		)
	endif()

	# More special stuff for OpenSSL which has a broken install target
	if(ARG_BROKEN_INSTALL)
		execute_process(
			COMMAND "${CMAKE_COMMAND}" -E env ${env}
				${make} ${make_flags}
			COMMAND_ECHO STDOUT
			WORKING_DIRECTORY "${source_dir}"
			COMMAND_ERROR_IS_FATAL ANY
		)
	endif()

	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E env ${env}
			${make} ${install} ${make_flags}
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
endfunction()

function(_build_cmake build_type target_bits source_dir)
	set(configure "${CMAKE_COMMAND}" -S "${source_dir}" -B .)
	cmake_parse_arguments(PARSE_ARGV 2 "CMAKE_ARG" "NO_DEFAULT_FLAGS;NO_DEFAULT_BUILD_TYPE" "" "")
	_parse_flags("${build_type}" "${source_dir}" configure configure_flags env ${CMAKE_ARG_UNPARSED_ARGUMENTS})

	if(WIN32 AND target_bits EQUAL 32)
		list(APPEND configure -A Win32)
	endif()

	if(build_type STREQUAL "debug")
		set(install_flags "--config;Debug")
	elseif(build_type STREQUAL "relwithdebinfo")
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
		list(JOIN CMAKE_PREFIX_PATH "\\;" prefix_path)
		set(default_flags
			"-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			"-DCMAKE_PREFIX_PATH=${prefix_path}"
			"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
		)
		if(NOT CMAKE_ARG_NO_DEFAULT_BUILD_TYPE AND USE_ASAN)
			list(APPEND default_flags
				"-DCMAKE_EXE_LINKER_FLAGS_INIT=-fno-omit-frame-pointer -fsanitize=address"
				"-DCMAKE_C_FLAGS_INIT=-fno-omit-frame-pointer -fsanitize=address"
				"-DCMAKE_CXX_FLAGS_INIT=-fno-omit-frame-pointer -fsanitize=address"
				"-DCMAKE_OBJC_FLAGS_INIT=-fno-omit-frame-pointer -fsanitize=address"
				"-DCMAKE_OBJCXX_FLAGS_INIT=-fno-omit-frame-pointer -fsanitize=address"
			)
		endif()
	endif()

	if(CMAKE_TOOLCHAIN_FILE)
		list(APPEND default_flags
			"-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
			"-DANDROID_ABI=${ANDROID_ABI}"
			"-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
			"-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=on"
		)
	elseif(CMAKE_ANDROID_NDK)
		list(APPEND default_flags
			"-DCMAKE_TOOLCHAIN_FILE=${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake"
			"-DCMAKE_ANDROID_NDK=${CMAKE_ANDROID_NDK}"
			"-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}"
			"-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
			"-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=on"
		)
	endif()

	if(CMAKE_ARG_NO_DEFAULT_FLAGS OR CMAKE_ARG_NO_DEFAULT_BUILD_TYPE)
		set(build_flag "")
	elseif(build_type STREQUAL "debug")
		set(build_flag "-DCMAKE_BUILD_TYPE=Debug")
	elseif(build_type STREQUAL "relwithdebinfo")
		set(build_flag "-DCMAKE_BUILD_TYPE=RelWithDebInfo")
	else()
		set(build_flag "-DCMAKE_BUILD_TYPE=Release")
	endif()

	set(binary_dir "${source_dir}-build")
	file(MAKE_DIRECTORY "${binary_dir}")
	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E env ${env}
			${configure}
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

	if(NOT KEEP_BINARY_DIRS)
		file(REMOVE_RECURSE "${binary_dir}")
	endif()
endfunction()

function(get_android_env _out_env _out_triplet ndk abi platform)
	if(abi STREQUAL "armeabi-v7a")
		set(triplet armv7a-linux-androideabi)
	elseif(abi STREQUAL "arm64-v8a")
		set(triplet aarch64-linux-android)
	elseif(abi STREQUAL "x86")
		set(triplet i686-linux-android)
	elseif(abi STREQUAL "x86_64")
		set(triplet x86_64-linux-android)
	else()
		message(FATAL_ERROR "Unknown Android ABI '${abi}'")
	endif()

	if(platform MATCHES "^android-([0-9]+)$")
		set(api ${CMAKE_MATCH_1})
	else()
		message(FATAL_ERROR "Unknown Android platform '${ANDROID_PLATFORM}'")
	endif()

	get_android_toolchain(toolchain "${ndk}")
	set(cc "${toolchain}/bin/${triplet}${api}-clang")

	set(${_out_env}
		"PATH=${toolchain}/bin:$ENV{PATH}"
		"TOOLCHAIN=${toolchain}"
		"TARGET=${triplet}"
		"AR=${toolchain}/bin/llvm-ar"
		"CC=${cc}"
		"AS=${cc}"
		"CXX=${cc}++"
		"LD=${toolchain}/bin/ld"
		"RANLIB=${toolchain}/bin/llvm-ranlib"
		"STRIP=${toolchain}/bin/llvm-strip"
		PARENT_SCOPE
	)
	set(${_out_triplet} ${triplet} PARENT_SCOPE)
endfunction()

function(get_android_toolchain out_var ndk)
	if(APPLE)
		set(kernel darwin)
	else()
		set(kernel linux)
	endif()

	set(${out_var} "${ndk}/toolchains/llvm/prebuilt/${kernel}-x86_64" PARENT_SCOPE)
endfunction()

function(_build_msbuild build_type target_bits source_dir)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "SOLUTION;SHARED;STATIC;DEBUG_SHARED;DEBUG_STATIC" "INCLUDES;RM")

	if(build_type STREQUAL "debug")
		if(NOT ARG_DEBUG_SHARED)
			message(FATAL_ERROR "Missing DEBUG_SHARED for ${ARG_SOLUTION}")
		endif()
		if(NOT ARG_DEBUG_STATIC)
			message(FATAL_ERROR "Missing DEBUG_STATIC for ${ARG_SOLUTION}")
		endif()
		set(ARG_SHARED "${ARG_DEBUG_SHARED}")
		set(ARG_STATIC "${ARG_DEBUG_STATIC}")
	endif()

	if(target_bits EQUAL 32)
		set(platform "Win32")
	elseif(target_bits EQUAL 64)
		set(platform "x64")
	else()
		message(FATAL_ERROR "Invalid target_bits '${target_bits}' for ${ARG_SOLUTION}")
	endif()

	execute_process(
		COMMAND msbuild -m
			"-p:Configuration=${ARG_SHARED};Platform=${platform};OutDir=${CMAKE_INSTALL_PREFIX}/bin"
			"${source_dir}/${ARG_SOLUTION}"
		COMMAND_ECHO STDOUT
		WORKING_DIRECTORY "${source_dir}"
		COMMAND_ERROR_IS_FATAL ANY
	)
	execute_process(
		COMMAND msbuild -m
			"-p:Configuration=${ARG_STATIC};Platform=${platform};OutDir=${CMAKE_INSTALL_PREFIX}/lib"
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
	_parse_flags("${build_type}" "${source_dir}" configure configure_flags env ${ARGN})

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
		COMMAND "${CMAKE_COMMAND}" -E env ${env}
			${configure}
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

	if(NOT KEEP_BINARY_DIRS)
		file(REMOVE_RECURSE "${binary_dir}")
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
				message(FATAL_ERROR "Hash computation failed!")
			elseif(NOT expected STREQUAL actual)
				message(FATAL_ERROR "Hash check failed! ${expected} != ${actual}")
			endif()
		else()
			set(algo SHA384)
			file(${algo} "${filename}" actual)
			message(WARNING "No hash available (${algo}=${actual})")
		endif()
	else()
		list(GET result 1 result)
		message(FATAL_ERROR "Failed: ${result}")
	endif()

	message(STATUS "Extracting ${filename}...")
	file(ARCHIVE_EXTRACT INPUT "${filename}")
	if(NOT KEEP_ARCHIVES)
		file(REMOVE "${filename}")
	endif()
endfunction()

function(_parse_flags build_type source_dir out_configurator out_flags out_env)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "CONFIGURATOR;ENV;ALL;DEBUG;RELWITHDEBINFO;RELEASE")

	if(ARG_CONFIGURATOR)
		if(NOT IS_ABSOLUTE "${ARG_CONFIGURATOR}")
			set(ARG_CONFIGURATOR ${source_dir}/${ARG_CONFIGURATOR})
		endif()
		string(CONFIGURE "${ARG_CONFIGURATOR}" configurator @ONLY)
		set(${out_configurator} ${configurator} PARENT_SCOPE)
	endif()

	if(ARG_ENV)
		set(${out_env} ${ARG_ENV} PARENT_SCOPE)
	endif()

	if(build_type STREQUAL "debug" AND ARG_DEBUG)
		set(${out_flags} "${ARG_DEBUG};${ARG_ALL}" PARENT_SCOPE)
	elseif(build_type STREQUAL "relwithdebinfo" AND ARG_RELWITHDEBINFO)
		set(${out_flags} "${ARG_RELWITHDEBINFO};${ARG_ALL}" PARENT_SCOPE)
	elseif(build_type STREQUAL "release" AND ARG_RELEASE)
		set(${out_flags} "${ARG_RELEASE};${ARG_ALL}" PARENT_SCOPE)
	else()
		set(${out_flags} "${ARG_ALL}" PARENT_SCOPE)
	endif()
endfunction()
