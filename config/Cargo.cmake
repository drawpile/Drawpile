function(add_cargo_executable target)
	_add_cargo(${target} TRUE ${ARGN})
endfunction()

function(add_cargo_library target)
	_add_cargo(${target} FALSE ${ARGN})
endfunction()

function(_add_cargo target is_exe)
	set(options ALL_FEATURES NO_DEFAULT_FEATURES)
	set(singleValueArgs PACKAGE MANIFEST_PATH)
	set(multiValueArgs FEATURES)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

	find_program(CARGO_COMMAND cargo REQUIRED
		PATHS $ENV{HOME}/.cargo/bin
		DOC "Cargo executable"
	)

	if(CMAKE_CROSSCOMPILING)
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|[Aa][Aa][Rr][Cc][Hh]64")
			set(arch aarch64)
		elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Mm][Dd]64|[Xx]86_64")
			set(arch x86_64)
		elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "[Ii]686")
			if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
				set(arch x86_64)
			else()
				set(arch i686)
			endif()
		endif()

		if(WIN32)
			set(sys pc-windows)
			if(MSVC)
				set(abi msvc)
			elseif(MINGW)
				set(abi gnu)
			endif()
		elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
			set(sys apple)
			set(abi darwin)
		elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
			set(sys unknown-linux)
			set(abi gnu)
		endif()

		if(NOT arch OR NOT sys OR NOT abi)
			message(WARNING "Unable to determine target ABI for Cargo; using host ABI")
		else()
			set(triple ${arch}-${sys}-${abi})
			message(STATUS "Cargo target ABI: ${triple}")
		endif()
	endif()

	set(cargo_dir ${CMAKE_CURRENT_BINARY_DIR}/cargo)
	set(cargo_target_dir "${cargo_dir}/$<$<BOOL:${triple}>:${triple}/>$<LOWER_CASE:$<CONFIG>>")

	if(is_exe)
		set(all ALL)
		set(out_name ${target}${CMAKE_EXECUTABLE_SUFFIX})
		set(out_kind RUNTIME)
	else()
		set(all "")
		set(out_name ${CMAKE_STATIC_LIBRARY_PREFIX}${ARG_PACKAGE}${CMAKE_STATIC_LIBRARY_SUFFIX})
		set(out_kind ARCHIVE)
	endif()

	if(CMAKE_${out_kind}_OUTPUT_DIRECTORY)
		set(out_dir ${CMAKE_${out_kind}_OUTPUT_DIRECTORY})
	else()
		set(out_dir ${CMAKE_CURRENT_BINARY_DIR})
	endif()

	if(ARG_PACKAGE)
		set(extra_comment " with packages ${ARG_PACKAGE}")
	endif()

	add_custom_target(cargo-build_${target}
		${all}
		BYPRODUCTS "${out_dir}/${out_name}"
		COMMAND ${CARGO_COMMAND} build
			"$<$<BOOL:${ARG_MANIFEST_PATH}>:--manifest-path;${ARG_MANIFEST_PATH}>"
			"$<$<BOOL:${ARG_FEATURES}>:--features;${ARG_FEATURES}>"
			"$<$<BOOL:${ARG_ALL_FEATURES}>:--all-features>"
			"$<$<BOOL:${ARG_NO_DEFAULT_FEATURES}>:--no-default-features>"
			"$<$<BOOL:${triple}>:--target;${triple}>"
			"$<$<BOOL:${ARG_PACKAGE}>:--package;${ARG_PACKAGE}>"
			"$<$<BOOL:${VERBOSE}>:--verbose>"
			--profile "$<IF:$<CONFIG:Debug>,dev,$<LOWER_CASE:$<CONFIG>>>"
			--target-dir ${cargo_dir}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cargo_target_dir}/${out_name}" "${out_dir}/${out_name}"
		COMMAND_EXPAND_LISTS
		COMMENT "Running cargo${extra_comment}"
		VERBATIM
	)

	# This does not consider manifest path and it does not follow the Cargo
	# rules about finding package directories and it does not work if someone
	# does not specify packages at all but it works for how this function is
	# used today, so YAGNI
	if(ARG_PACKAGE AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
		foreach(item IN LISTS ARG_PACKAGE)
			file(GLOB_RECURSE sources ${CMAKE_SOURCE_DIR}/src/${item}/*)
			target_sources(cargo-build_${target} PRIVATE ${sources})
			source_group(TREE "${CMAKE_SOURCE_DIR}/src/${item}" PREFIX "Source Files" FILES ${sources})
		endforeach()
	endif()

	if(is_exe)
		add_executable(${target} IMPORTED GLOBAL)
	else()
		add_library(${target} STATIC IMPORTED GLOBAL)

		# Adapted from corrosion. Required for successful linking of anything
		# that uses the Rust Standard Library
		set(libs "")
		if(WIN32)
			list(APPEND libs "advapi32" "userenv" "ws2_32" "bcrypt")
			if(MSVC)
				list(APPEND libs "$<$<CONFIG:Debug>:msvcrtd>")
				# CONFIG takes a comma seperated list starting with CMake 3.19, but we still need to
				# support older CMake versions.
				set(config_is_release "$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>")
				list(APPEND libs "$<${config_is_release}:msvcrt>")
			else()
				list(APPEND libs "gcc_eh" "pthread")
			endif()
		elseif(APPLE)
			list(APPEND libs "System" "resolv" "c" "m")
		else()
			list(APPEND libs "dl" "rt" "pthread" "gcc_s" "c" "m" "util")
		endif()

		set_target_properties(${target} PROPERTIES
			INTERFACE_LINK_LIBRARIES "${libs}"
		)
	endif()

	add_dependencies(${target} cargo-build_${target})
	set_target_properties(${target} PROPERTIES
		IMPORTED_LOCATION ${out_dir}/${out_name}
		ADDITIONAL_CLEAN_FILES ${cargo_dir}
	)
endfunction()
