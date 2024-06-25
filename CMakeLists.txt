cmake_minimum_required(VERSION 3.18)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. Please specify a"
	" build directory when running CMake"
	" (`${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B <dir_name>`).\n"
	"You should also delete these files, which have already been generated:\n"
	"* ${CMAKE_BINARY_DIR}/CMakeCache.txt\n"
	"* ${CMAKE_BINARY_DIR}/CMakeFiles")
endif()

# These flags are nonsense, Drawpile only uses libraries for internal assembly,
# we don't want to install any into the system. It may also be possible to add
# STATIC to every add_library call, but not sure if that doesn't break on some
# platform, so just purge these flags to not let them mess with our build.
foreach(garbage_flag IN ITEMS BUILD_SHARED_LIBS BUILD_STATIC_LIBS)
	if(DEFINED "${garbage_flag}")
		message(WARNING "Ignoring nonsensical '${garbage_flag}' argument")
		unset("${garbage_flag}")
		unset("${garbage_flag}" CACHE)
	endif()
endforeach()

# This must come before `project` because the version it pulls from an external
# file is used as the CMake project version
include(DrawpileVersions)

project(Drawpile
	VERSION ${PROJECT_VERSION}
	HOMEPAGE_URL https://drawpile.net
	LANGUAGES C CXX
)

include(DrawpileCheckSymlinks)

# This must come after `project` because cross-compilation is configured by the
# `project` call
if(APPLE)
	enable_language(OBJCXX)
endif()

# CMake includes warning and exception handling flags into the MSVC command line
# by default, which in turn causes MSVC to whinge about us overriding them with
# every file that's compiled. We just delete those garbage flags out of the
# default command line, that's apparently the "normal" solution.
if(MSVC)
	message(STATUS "CMAKE_C_FLAGS before cleanup: ${CMAKE_C_FLAGS}")
	message(STATUS "CMAKE_CXX_FLAGS before cleanup: ${CMAKE_CXX_FLAGS}")
	message(STATUS "CMAKE_OBJCXX_FLAGS before cleanup: ${CMAKE_OBJCXX_FLAGS}")
	string(REGEX REPLACE "/EH[cs-]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REGEX REPLACE "/EH[cs-]+" "" CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS}")
	string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REGEX REPLACE "/W[0-4]" "" CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS}")
	message(STATUS "CMAKE_C_FLAGS before cleanup: ${CMAKE_C_FLAGS}")
	message(STATUS "CMAKE_CXX_FLAGS after cleanup: ${CMAKE_CXX_FLAGS}")
	message(STATUS "CMAKE_OBJCXX_FLAGS after cleanup: ${CMAKE_OBJCXX_FLAGS}")
endif()

# DrawpileOptions uses these so it must be included before then
include(Cargo)
include(FeatureSummary)

# This must come after `project` because it relies on variables like `ANDROID`
# that are only set once that is called
include(DrawpileOptions)

# This cannot go in `DrawpileVersions.cmake` because it relies on stuff from
# `DrawpileOptions`
if(CLIENT OR SERVERGUI)
	set(DP_MIN_QT_VERSION 5.12)
else()
	# The minimum for headless servers is different than GUI because the current
	# Debian LTS provides only this older version of Qt, but it is too painful
	# to not have at least 5.12 when doing GUI stuff
	set(DP_MIN_QT_VERSION 5.11)
endif()

# This check must happen after `project()` because `CMAKE_CONFIGURATION_TYPES`
# is not populated until then
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(FATAL_ERROR "Required build type missing."
		" Re-run CMake and specify one of these build types:\n"
		"* -DCMAKE_BUILD_TYPE=Debug\n"
		"* -DCMAKE_BUILD_TYPE=Release\n"
		"* -DCMAKE_BUILD_TYPE=RelWithDebInfo\n"
		"* -DCMAKE_BUILD_TYPE=MinSizeRel")
endif()

if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	message(WARNING "Using ${PROJECT_NAME} as a subproject is untested.")
endif()

# CMake does not allow labels on the version in the project command, but having
# the split version number is useful for other places, so just pretend like it
# does semver and maybe it will someday
set(PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_LABEL}")
if(BUILD_LABEL)
	set(PROJECT_VERSION "${PROJECT_VERSION}+${BUILD_LABEL}")
endif()

# All of this information should be known by this point
message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Protocol version: ${DRAWPILE_PROTO_SERVER_VERSION}.${DRAWPILE_PROTO_MAJOR_VERSION}.${DRAWPILE_PROTO_MINOR_VERSION}")

# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html

if(QT_VERSION)
    find_package(QT ${DP_MIN_QT_VERSION} NAMES "Qt${QT_VERSION}" REQUIRED)
else()
    find_package(QT ${DP_MIN_QT_VERSION} NAMES Qt6 Qt5 REQUIRED)
endif()
message(STATUS "Using Qt version ${QT_VERSION_MAJOR}")
set(QT_PACKAGE_NAME Qt${QT_VERSION_MAJOR})
# Qt5AndroidSupport.cmake clobbers this variable, so make a copy
set(DP_QT_DIR ${QT_DIR})

# There is an inherent conflict where the docs say `CMAKE_OSX_DEPLOYMENT_TARGET`
# needs to be set before calling `enable_language` or `project`, but it is
# impossible to run `find_package` before calling one of those and we need to
# do that to learn which Qt version we are using since that defines what the
# minimum deployment target is. So, we default it to the absolute minimum at the
# start of the file, and then increase it to whatever Qt requires here
include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

include(AutoSourceGroup)
include(DrawpileInstallDirs)

# This *must* be included before `find_package(Qt5 COMPONENTS Core)` because
# `find_package` loads `Qt5AndroidSupport.cmake` which immediately creates the
# `apk` target and writes out `android_deployment_settings.json` using global
# variables.
if(ANDROID AND QT_VERSION VERSION_LESS 6)
	include(Qt5AndroidDeploymentTarget)
endif()

if(CLIENT OR SERVER)
	# Finding Qt needs to come first, otherwise automoc gets confused about
	# being unable to generate a MOC for the cmake-config directory below.
	find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Core Network)
endif()

# This must be included *after* drawdance is added to make sure that we do not
# pollute that sub-project with our compiler options. Once CMake 3.25 is the
# minimum required version and Qt5 for Android is not used any more, it is
# possible that the SYSTEM flag of `add_subdirectory` may do this and then this
# can be moved down the list to where the rest of the dependencies are.
include(GetIgnoreWarningsInDirectory)
include(DrawpileCompilerOptions)
include(Signing)

if(TESTS)
	enable_testing()
	include(Tests)
endif()

# All intra-project includes should be fully qualified relative to the root
# source directory to not go insane figuring out what is included from where
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

# Error messages shouldn't include home directories and such, so we use the
# project directory length to chop it off the beginning of __FILE__.
string(LENGTH "${PROJECT_SOURCE_DIR}/" project_dir_length)
add_compile_definitions("DP_PROJECT_DIR_LENGTH=${project_dir_length}")

add_subdirectory(src/cmake-config)

if(CLIENT OR SERVER)
	# Everything needs these dependencies so just do it in one place
	find_package(${QT_PACKAGE_NAME} QUIET COMPONENTS LinguistTools)
	find_package(libsodium QUIET)
	if(EMSCRIPTEN)
		find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS WebSockets)
	else()
		find_package(${QT_PACKAGE_NAME} QUIET COMPONENTS WebSockets)
	endif()

	if(ANDROID AND QT_VERSION VERSION_LESS 6)
		find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS AndroidExtras)
	endif()

	if(TARGET libsodium::libsodium)
		add_compile_definitions(HAVE_LIBSODIUM)
	endif()

	if(HAVE_TCPSOCKETS)
		add_compile_definitions(HAVE_TCPSOCKETS)
	endif()

	if(TARGET "${QT_PACKAGE_NAME}::WebSockets")
		add_compile_definitions(HAVE_WEBSOCKETS)
	endif()

	# LinguistTools does not generate a target so its existence must be checked
	# using the old-style FOUND flag
	add_feature_info("Translations" ${QT_PACKAGE_NAME}LinguistTools_FOUND "")
	add_feature_info("Ext-auth support" "TARGET libsodium::libsodium" "")
	add_feature_info("WebSocket support" "TARGET ${QT_PACKAGE_NAME}::WebSockets" "")

	message(STATUS "Adding drawdance")
	add_subdirectory(src/drawdance)

	message(STATUS "Adding libshared")
	add_subdirectory(src/libshared)
endif()

if(CLIENT)
	message(STATUS "Adding libclient")
	add_subdirectory(src/libclient)

	message(STATUS "Adding desktop")
	add_subdirectory(src/desktop)
endif()

if(SERVER OR BUILTINSERVER)
	message(STATUS "Adding libserver")
	add_subdirectory(src/libserver)
endif()

if(SERVER)
	message(STATUS "Adding thinsrv")
	add_subdirectory(src/thinsrv)
endif()

if(TOOLS)
	message(STATUS "Adding tools")
	add_subdirectory(src/tools)
endif()

if(CARGO_COMMAND)
	add_custom_target(clippy
		COMMAND "${CARGO_COMMAND}" clippy --
			# This lint isn't feasible in such a C-heavy codebase, since they
			# would just end up marking every function in the universe unsafe,
			# meaning we wouldn't get any of Rust's guarantees anywhere.
			-A clippy::not_unsafe_ptr_arg_deref
			# There's no useful safety docs that we could apply on a function level.
			-A clippy::missing_safety_doc
			# Too many positional arguments to a function isn't nice, but making a
			# struct to emulate named arguments is even worse.
			-A clippy::too_many_arguments
			# Turn on some of these restrictions for more consistency.
			-W clippy::empty_structs_with_brackets
			-W clippy::shadow_reuse
			-W clippy::shadow_same
			-W clippy::string_to_string
			-W clippy::try_err
			-W clippy::unseparated_literal_suffix
			-W clippy::verbose_file_reads
			# Turn on pedantic warnings, but disable the silly ones.
			-W clippy::pedantic
			# False positives when the string has already been lowercased.
			-A clippy::case_sensitive_file_extension_comparisons
			# If you didn't want these, you wouldn't be using `as` to cast.
			-A clippy::cast_possible_truncation
			-A clippy::cast_possible_wrap
			-A clippy::cast_precision_loss
			-A clippy::cast_sign_loss
			# We don't ever want to panic in a valid case, so nothing to document.
			-A clippy::missing_panics_doc
			# We do this only on extern "C" types, where it's necessary to do so.
			-A clippy::module_name_repetitions
			# Nothing needless about explicitly consuming a value.
			-A clippy::needless_pass_by_value
			# Single characters are fine, we use them for w and h or r, g, b and a.
			-A clippy::many_single_char_names
			# We don't have exciting error handling, so nothing to document.
			-A clippy::missing_errors_doc
			# False positives and not terribly useful a lint to begin with.
			-A clippy::must_use_candidate
			# Similar names are better than inconsistent names.
			-A clippy::similar_names
			# Artificially splitting up functions doesn't make them easier to read.
			-A clippy::too_many_lines
			# Putting the format arguments after the string often looks nicer.
			-A clippy::uninlined_format_args
			# False positives on argb literals that are perfectly readable.
			-A clippy::unreadable_literal
	)
endif()

# This must run once all target creation is finished since it walks the list of
# targets and their sources. This could be called any time and use
# `cmake_language(DEFER)` once CMake 3.19 is the minimum supported version.
disable_automoc_warnings()

# Since Android is cross-compiled it is already always creating a distributable
# build
if(DIST_BUILD AND NOT ANDROID)
	include(DrawpileDistBuild)
endif()

include(DrawpilePackaging)

feature_summary(WHAT PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)

if(CMAKE_CONFIGURATION_TYPES)
	message(NOTICE
		"++ For a debug build, run: `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}"
		" --config Debug`\n"
		"++ For a release build, run: `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}"
		" --config Release`")
else()
	message(NOTICE
		"++ To build, run: `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}`")
endif()
