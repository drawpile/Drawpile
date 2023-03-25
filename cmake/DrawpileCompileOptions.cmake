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

if (MSVC)
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
	list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "/W([0-9]{4})" "/wn\\1")
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

foreach(lang IN ITEMS C CXX OBJC OBJCXX)
	if(lang IN_LIST ENABLED_LANGUAGES AND NOT CMAKE_${lang}_CLANG_TIDY)
		set(clang_tidy_names clang-tidy)
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			string(REGEX MATCH "^([0-9]+)" clang_tidy_version_matched "${CMAKE_CXX_COMPILER_VERSION}")
			if(CMAKE_MATCH_1)
				list(APPEND clang_tidy_names "clang-tidy-${CMAKE_MATCH_1}")
			endif()
		endif()
		find_program(clang_tidy NAMES ${clang_tidy_names})
		if(clang_tidy)
			set(CMAKE_CXX_CLANG_TIDY "${clang_tidy}")
		endif()
	endif()
endforeach()
add_feature_info("Clang-Tidy" CMAKE_CXX_CLANG_TIDY "")
