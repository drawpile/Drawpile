# SPDX-License-Identifier: MIT
find_program(CARGO_COMMAND cargo REQUIRED
	PATHS $ENV{HOME}/.cargo/bin
	DOC "Cargo executable"
)

# SPDX-SnippetBegin
# SPDX-License-Identifier: MIT
# SDPX—SnippetName: static libs detection from corrosion
function(_corrosion_determine_libs_new target_triple out_libs)
    set(package_dir "${CMAKE_BINARY_DIR}/corrosion/required_libs")
    # Cleanup on reconfigure to get a cleans state (in case we change something in the future)
    file(REMOVE_RECURSE "${package_dir}")
    file(MAKE_DIRECTORY "${package_dir}")
    set(manifest "[package]\nname = \"required_libs\"\nedition = \"2018\"\nversion = \"0.1.0\"\n")
    string(APPEND manifest "\n[lib]\ncrate-type=[\"staticlib\"]\npath = \"lib.rs\"\n")
    string(APPEND manifest "\n[workspace]\n")
    file(WRITE "${package_dir}/Cargo.toml" "${manifest}")
    file(WRITE "${package_dir}/lib.rs" "pub fn add(left: usize, right: usize) -> usize {left + right}\n")

	set(args rustc --verbose --color never)
	if(target_triple)
		list(APPEND args --target=${target_triple})
	endif()
	list(APPEND args -- --print=native-static-libs)
	execute_process(
		COMMAND ${CARGO_COMMAND} ${args}
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/corrosion/required_libs"
		RESULT_VARIABLE cargo_build_result
		ERROR_VARIABLE cargo_build_error_message
	)

    if(cargo_build_result)
        message(SEND_ERROR "Determining required native libraries - failed: ${cargo_build_result}.")
        message(SEND_ERROR "The cargo build error was: ${cargo_build_error_message}")
        return()
    else()
        # The pattern starts with `native-static-libs:` and goes to the end of the line.
        if(cargo_build_error_message MATCHES "native-static-libs: ([^\r\n]+)\r?\n")
            string(REPLACE " " ";" "libs_list" "${CMAKE_MATCH_1}")
            set(stripped_lib_list "")
            foreach(lib ${libs_list})
                # Strip leading `-l` (unix) and potential .lib suffix (windows)
                string(REGEX REPLACE "^-l" "" "stripped_lib" "${lib}")
                string(REGEX REPLACE "\.lib$" "" "stripped_lib" "${stripped_lib}")
                list(APPEND stripped_lib_list "${stripped_lib}")
            endforeach()
            set(libs_list "${stripped_lib_list}")
            # Special case `msvcrt` to link with the debug version in Debug mode.
            list(TRANSFORM libs_list REPLACE "^msvcrt$" "\$<\$<CONFIG:Debug>:msvcrtd>")
        else()
            message(SEND_ERROR "Determining required native libraries - failed: Regex match failure.")
            message(SEND_ERROR "`native-static-libs` not found in: `${cargo_build_error_message}`")
            return()
        endif()
    endif()
    set("${out_libs}" "${libs_list}" PARENT_SCOPE)
endfunction()
# SPDX-SnippetEnd

function(_cargo_set_libs target triple)
	if("${triple}")
		set(cache_var "CARGO_LIBS_${triple}")
	else()
		set(cache_var "CARGO_LIBS_HOST")
	endif()

	set(libs "${${cache_var}}")
	if(libs)
		message(STATUS "Re-used Rust libraries: ${libs}")
	else()
		_corrosion_determine_libs_new("${triple}" libs)
		list(REMOVE_DUPLICATES libs)
		set("${cache_var}" "${libs}" CACHE INTERNAL "")
		message(STATUS "Determined Rust libraries: ${libs}")
	endif()

	set_target_properties("${target}" PROPERTIES
		INTERFACE_LINK_LIBRARIES "${libs}"
	)
endfunction()

#[[
Adds an imported Rust static library target that is built by Cargo.
#]]
function(add_cargo_library target package)
	set(options ALL_FEATURES NO_DEFAULT_FEATURES)
	set(singleValueArgs MANIFEST_PATH)
	set(multiValueArgs FEATURES)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

	if(CARGO_TRIPLE)
		set(triple "${CARGO_TRIPLE}")
	elseif(CMAKE_CROSSCOMPILING)
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|[Aa][Aa][Rr][Cc][Hh]64")
			set(arch aarch64)
		elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM32|[Aa][Rr][Mm][Vv]7")
			set(arch armv7)
		elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Mm][Dd]64|[Xx]86_64")
			set(arch x86_64)
		elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "[Ii]686")
			if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
				set(arch x86_64)
			else()
				set(arch i686)
			endif()
		else()
			message(WARNING "Unknown Cargo arch for processor '${CMAKE_SYSTEM_PROCESSOR}'")
		endif()

		if(WIN32)
			set(sys pc-windows)
			if(MSVC)
				set(abi msvc)
			elseif(MINGW)
				set(abi gnu)
			endif()
		elseif(ANDROID)
			set(sys linux)
			if("${arch}" STREQUAL "armv7")
				set(abi androideabi)
			else()
				set(abi android)
			endif()
		elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
			set(sys apple)
			set(abi darwin)
		elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
			set(sys unknown-linux)
			set(abi gnu)
		else()
			message(WARNING "Unknown Cargo sys and abi for system '${CMAKE_SYSTEM_NAME}'")
		endif()

		if(NOT arch OR NOT sys OR NOT abi)
			message(SEND_ERROR "Unable to determine target ABI for Cargo")
		else()
			set(triple ${arch}-${sys}-${abi})
			message(STATUS "Cargo target ABI: ${triple}")
		endif()
	endif()

	set(cargo_dir ${CMAKE_CURRENT_BINARY_DIR}/cargo)
	set(cargo_target_dir "${cargo_dir}/$<$<BOOL:${triple}>:${triple}/>$<LOWER_CASE:$<CONFIG>>")

	set(out_name ${CMAKE_STATIC_LIBRARY_PREFIX}${package}${CMAKE_STATIC_LIBRARY_SUFFIX})
	if(CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
		set(out_dir ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
	else()
		set(out_dir ${CMAKE_CURRENT_BINARY_DIR})
	endif()

	add_custom_target(cargo-build_${target}
		BYPRODUCTS "${out_dir}/${out_name}"
		COMMAND ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
			${CARGO_COMMAND} build
			"$<$<BOOL:${ARG_MANIFEST_PATH}>:--manifest-path;${ARG_MANIFEST_PATH}>"
			"$<$<BOOL:${ARG_FEATURES}>:--features;${ARG_FEATURES}>"
			"$<$<BOOL:${ARG_ALL_FEATURES}>:--all-features>"
			"$<$<BOOL:${ARG_NO_DEFAULT_FEATURES}>:--no-default-features>"
			"$<$<BOOL:${triple}>:--target;${triple}>"
			"$<$<BOOL:${package}>:--package;${package}>"
			"$<$<BOOL:${VERBOSE}>:--verbose>"
			--manifest-path "${PROJECT_SOURCE_DIR}/Cargo.toml"
			--profile "$<IF:$<CONFIG:Debug>,dev,$<LOWER_CASE:$<CONFIG>>>"
			--target-dir ${cargo_dir}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${cargo_target_dir}/${out_name}" "${out_dir}/${out_name}"
		COMMAND_EXPAND_LISTS
		COMMENT "Running cargo with package ${package}"
		VERBATIM
	)

	# This does not consider manifest path and it does not follow the Cargo
	# rules about finding package directories and it does not work if someone
	# does not specify packages at all but it works for how this function is
	# used today, so YAGNI
	if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
		foreach(item IN LISTS package)
			file(GLOB_RECURSE sources ${PROJECT_SOURCE_DIR}/src/${item}/*)
			target_sources(cargo-build_${target} PRIVATE ${sources})
			source_group(TREE "${PROJECT_SOURCE_DIR}/src/${item}" PREFIX "Source Files" FILES ${sources})
		endforeach()
	endif()

	add_library(${target} STATIC IMPORTED GLOBAL)

	_cargo_set_libs("${target}" "${triple}")

	add_dependencies(${target} cargo-build_${target})
	set_target_properties(${target} PROPERTIES
		IMPORTED_LOCATION ${out_dir}/${out_name}
		ADDITIONAL_CLEAN_FILES ${cargo_dir}
	)
endfunction()
