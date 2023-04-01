#[[ This module sets up all of the C-family languages’ compiler options. #]]

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_PATH_PREFIX ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT CMAKE_INTERPROCEDURAL_OPTIMIZATION LANGUAGES CXX)
endif()
add_feature_info("Interprocedural optimization (CMAKE_INTERPROCEDURAL_OPTIMIZATION)" CMAKE_INTERPROCEDURAL_OPTIMIZATION "")

if(MSVC)
	add_compile_options(/utf-8 /W4)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

	if($ENV{CI})
		add_compile_options(/WX)
		add_link_options(/WX:NO)
	endif()

	# https://github.com/mozilla/sccache/pull/963
	if(CMAKE_C_COMPILER_LAUNCHER MATCHES "sccache")
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
			string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
		endif()
	endif()
	if(CMAKE_CXX_COMPILER_LAUNCHER MATCHES "sccache")
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
			string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
		elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
		endif()
	endif()

	get_directory_property(IGNORE_WARNINGS_COMPILE_OPTIONS COMPILE_OPTIONS)
	list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "/W[0-9]$" "/W0")
	list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "/W([0-9]{4})" "/wd\\1")
else()
	add_compile_options(-Wall -Wextra -Wpedantic
		-Wcast-align
		-Wcast-qual
		-Wextra-semi
		-Wformat-nonliteral
		-Wimplicit-fallthrough
		-Wnon-virtual-dtor
		-Wold-style-cast
		-Woverloaded-virtual
		-Wshadow
		-Wunused
		-Wunused-parameter
		-Wunused-macros
		-Wzero-as-null-pointer-constant
	)

	# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105725
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.2)
		add_compile_options(-Wmismatched-tags)
	endif()

	include(CheckCXXCompilerFlag)
	foreach(flag IN ITEMS
		-Warray-bounds-pointer-arithmetic
		-Walloc-zero
		-Warray-bounds=2
		-Wdeprecated
		-Wduplicated-branches
		-Wduplicated-cond
		-Wformat-pedantic
		-Wformat-security
		-Wlogical-op
		-Wlogical-op-parentheses
		-Wsuggest-destructor-override
		-Wsuggest-final-methods
		-Wsuggest-final-types
		-Wsuggest-override
		-Wunused-member-function
		-Wunused-template
		-Wuseless-cast
	)
		string(TOUPPER ${flag} flag_name)
		string(REPLACE "-W" "CXX_HAS_" flag_name ${flag_name})
		string(REPLACE "-" "_" flag_name ${flag_name})
		string(REPLACE "=" "_" flag_name ${flag_name})
		check_cxx_compiler_flag(${flag} ${flag_name})
		if(${flag_name})
			add_compile_options(${flag})
		endif()
	endforeach()

	if($ENV{CI})
		add_compile_options(-Werror)
	endif()

	get_directory_property(IGNORE_WARNINGS_COMPILE_OPTIONS COMPILE_OPTIONS)
	list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "-W(no-)?([^=]+)(=.*$)?" "-Wno-\\2")
endif()

if(CLANG_TIDY)
	get_property(enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
	foreach(lang IN LISTS enabled_languages)
		if(NOT CMAKE_${lang}_CLANG_TIDY)
			if(NOT DEFINED clang_tidy_exe)
				set(clang_tidy_names clang-tidy)
				if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
					string(REGEX MATCH "^([0-9]+)" clang_tidy_version_matched "${CMAKE_CXX_COMPILER_VERSION}")
					if(CMAKE_MATCH_1)
						list(PREPEND clang_tidy_names "clang-tidy-${CMAKE_MATCH_1}")
					endif()
				endif()
				find_program(clang_tidy_exe NAMES ${clang_tidy_names})
			endif()
			if(clang_tidy_exe)
				set(CMAKE_${lang}_CLANG_TIDY "${clang_tidy_exe}")
				list(APPEND IGNORE_TIDY_PROPERTY_NAMES ${lang}_CLANG_TIDY)
			endif()
		endif()
	endforeach()
	unset(enabled_languages)
	unset(clang_tidy_exe)
endif()
add_feature_info("Clang-Tidy (CLANG_TIDY)" CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}")

if(ADDRESS_SANITIZER)
	include(CheckLinkerFlag)
	check_linker_flag(CXX "-fsanitize=address" HAVE_SANITIZE_ADDRESS)
	if(HAVE_SANITIZE_ADDRESS)
		# In at least CMake 3.19, `add_compile_options` flags are used by
		# `try_compile` but `add_link_options` flags are not, so to prevent
		# accidental failures of feature detection caused by ASan linker
		# failures, explicitly set `CMAKE_EXE_LINKER_FLAGS` instead, which does
		# get used by `try_compile`.
		# Also, this variable is treated as a string, not a list, so using
		# `list(APPEND)` would break the compiler.
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
		add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
	else()
		set(ADDRESS_SANITIZER OFF)
	endif()
endif()
add_feature_info("AddressSanitizer (ADDRESS_SANITIZER)" ADDRESS_SANITIZER "")

#[[
Disables compiler warnings for the given source files.
#]]
function(source_files_disable_warnings target)
	set_source_files_properties(
		${ARGN}
		TARGET_DIRECTORY "${target}"
		PROPERTIES
		COMPILE_OPTIONS "${IGNORE_WARNINGS_COMPILE_OPTIONS}"
	)
endfunction()

#[[
Disables compiler warnings for `AUTOMOC` files for all targets in the current
source directory and all subdirectories.
#]]
function(disable_automoc_warnings)
	include(GetAllTargets)
	get_all_targets(targets ${CMAKE_CURRENT_SOURCE_DIR})
	target_disable_automoc_warnings(${targets})
endfunction()

#[[
Disables compiler warnings for `AUTOMOC` files in the given targets.

Such warnings are unhelpful: either they are coming from the autogenerated code
and nothing can be done, or they come from headers that are going to be included
again in another TU later and so it is not necessary to be warned twice.
#]]
function(target_disable_automoc_warnings)
	foreach(target IN LISTS ARGN)
		get_target_property(has_automoc ${target} AUTOMOC)
		get_target_property(binary_dir ${target} BINARY_DIR)
		if(has_automoc)
			set_source_files_properties(
				"${binary_dir}/${target}_autogen/mocs_compilation.cpp"
				TARGET_DIRECTORY "${target}"
				PROPERTIES COMPILE_OPTIONS "${IGNORE_WARNINGS_COMPILE_OPTIONS}"
			)
		endif()
	endforeach()
endfunction()
