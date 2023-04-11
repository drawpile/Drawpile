# SPDX-License-Identifier: MIT

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ANDROID OR EMSCRIPTEN)
    # This is required to use CMAKE_PREFIX_PATH for find_package in config mode,
    # otherwise it will only look in the sysroot cache
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ON)
endif()

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    # This flag is required when compiling all objects or linking will fail
    # when --shared-memory is used, which it is implicitly
    add_compile_options(-pthread)
endif()

if(MSVC)
    # C4100: unreferenced formal parameter
    #  - MSVC has no `__attribute__((unused))` and omitting parameter names
    #    wouldn't be standard C
    # C4127: conditional expression is constant
    #  - macros from uthash emit `do {} while(0);` and this triggers on
    #    `while(0)`
    # C4200: nonstandard extension used: zero-sized array in struct/union
    #  - MSVC does not recognise C99 flexible array members as standard
    # C4701 and 4703: potentially ininitialized (pointer) variable used
    #  - detection is so primitive that it spams innumerable false positives
    # C4702: unreachable code
    #  - macros from uthash causing problems again
    add_compile_options(
        /utf-8 /W4 /wd4100 /wd4127 /wd4200 /wd4701 /wd4702 /wd4703)
    if(ENABLE_WERROR)
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
else()
    add_compile_options(
        -Wpedantic -Wall -Wextra -Wshadow
        -Wmissing-include-dirs -Wconversion
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    )

    if(NOT USE_STRICT_ALIASING)
        add_compile_options(-fno-strict-aliasing)
        add_compile_definitions(DP_NO_STRICT_ALIASING)
    endif()

    if(ENABLE_ARCH_NATIVE)
        add_compile_options(-march=native)
    endif()
endif()

if(CLANG_TIDY)
    get_property(enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
    foreach(lang IN LISTS enabled_languages)
        if(NOT CMAKE_${lang}_CLANG_TIDY)
            if(NOT DEFINED clang_tidy_exe)
                set(clang_tidy_names clang-tidy)
                # At least Debian allows side-by-side compiler installations
                # with version suffixes, so prefer a suffixed version if the
                # current compiler is one of the versioned ones
                if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER MATCHES "-([0-9]+)$")
                    list(PREPEND clang_tidy_names "clang-tidy-${CMAKE_MATCH_1}")
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

# When LTO is enabled, CMake will try to pass -fno-fat-lto-objects, which in
# turn causes clang-tidy to spew a warning about not understanding it, which
# fails the compilation due to -Werror. We have to turn off the ignored
# optimization argument warning entirely, -Wno-error doesn't work with GCC
# in turn because it doesn't know that warning and dies because of it. At
# the time of writing, there's no way to make CMake not pass that flag.
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION AND CMAKE_CXX_CLANG_TIDY)
    add_compile_options(-Wno-ignored-optimization-argument)
endif()

if(MSVC)
    get_directory_property(IGNORE_WARNINGS_COMPILE_OPTIONS COMPILE_OPTIONS)
    list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "/W[0-9]$" "/W0")
    list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "/W([0-9]{4})" "/wd\\1")
else()
    get_directory_property(IGNORE_WARNINGS_COMPILE_OPTIONS COMPILE_OPTIONS)
    list(TRANSFORM IGNORE_WARNINGS_COMPILE_OPTIONS REPLACE "-W(no-)?([^=]+)(=.*$)?" "-Wno-\\2")
endif()
