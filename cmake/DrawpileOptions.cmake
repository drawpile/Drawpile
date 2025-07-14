#[[ This module defines all of the exposed build options. #]]

include(CMakeDependentOption)

option(CLIENT "Compile client" ON)
add_feature_info("Drawpile client (CLIENT)" CLIENT "")

# These don't not really need to be reported as a feature
set(BUILD_LABEL "" CACHE STRING "A custom label to add to the version")
set(BUILD_VERSION "" CACHE STRING "Version to use, instead of asking git")
set(BUILD_PACKAGE_SUFFIX "" CACHE STRING "Suffix to append to cpack artifacts")
if(ANDROID)
    set(BUILD_ANDROID_VERSION_CODE "" CACHE STRING "Android version code")
    add_feature_info("Android version code (BUILD_ANDROID_VERSION_CODE)" ON "${BUILD_ANDROID_VERSION_CODE}")
endif()

option(UPDATE_TRANSLATIONS "Update translation files from source")
add_feature_info(".ts regeneration (slow!) (UPDATE_TRANSLATIONS)" UPDATE_TRANSLATIONS "")

if(EMSCRIPTEN)
	set(HAVE_TCPSOCKETS OFF)
else()
	set(HAVE_TCPSOCKETS ON)
endif()
add_feature_info("TCP socket support" HAVE_TCPSOCKETS "")

if(NOT ANDROID AND NOT EMSCRIPTEN)
	cmake_dependent_option(SERVER "Compile dedicated server" OFF "HAVE_TCPSOCKETS" OFF)
	add_feature_info("Drawpile server (SERVER)" SERVER "")

	cmake_dependent_option(
		BUILTINSERVER "Compile builtin server for hosting from client"
		ON "CLIENT;HAVE_TCPSOCKETS" OFF)
	add_feature_info("Builtin server (BUILTINSERVER)" BUILTINSERVER "")

	cmake_dependent_option(SERVERGUI "Enable server GUI" ON "SERVER" OFF)
	add_feature_info("Server GUI (SERVERGUI)" SERVERGUI "")

	cmake_dependent_option(
		TOOLS "Command-line tools" OFF "CARGO_COMMAND" OFF)
	add_feature_info("Command-line tools, requires Rust (TOOLS)" TOOLS "")

	option(TESTS "Build unit tests" OFF)
	add_feature_info("Unit tests (TESTS)" TESTS "")

	option(BENCHMARKS "Build benchmarks" OFF)
	add_feature_info("Benchmarks (BENCHMARKS)" BENCHMARKS "")
else()
	# CMake allows unexposed options to be enabled
	set(SERVER OFF CACHE BOOL "" FORCE)
	set(SERVERGUI OFF CACHE BOOL "" FORCE)
	set(BUILTINSERVER OFF CACHE BOOL "" FORCE)
	set(TOOLS OFF CACHE BOOL "" FORCE)
	set(TESTS OFF CACHE BOOL "" FORCE)
	set(BENCHMARKS OFF CACHE BOOL "" FORCE)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
	cmake_dependent_option(INSTALL_DOC "Install documentation" ON "SERVER" OFF)
	add_feature_info("Install documentation (INSTALL_DOC)" INSTALL_DOC "")

	# Feature info will be emitted later once it is clear whether or not the
	# feature could actually be enabled
	cmake_dependent_option(INITSYS "Init system integration for server (options: \"\", \"systemd\")" "systemd" "SERVER" "")
	string(TOLOWER "${INITSYS}" INITSYS)
else()
	# CMake allows unexposed options to be enabled
	set(INSTALL_DOC OFF CACHE BOOL "" FORCE)
	set(INITSYS "" CACHE STRING "" FORCE)
endif()

if(NOT CMAKE_CROSSCOMPILING)
	option(USE_GENERATORS "Do code generation" ON)
	add_feature_info("Code generation (USE_GENERATORS)" USE_GENERATORS "")
endif()

if(NOT MSVC AND NOT EMSCRIPTEN)
	option(USE_STRICT_ALIASING "Enable strict aliasing optimizations" OFF)
	add_feature_info("Strict aliasing (USE_STRICT_ALIASING)" USE_STRICT_ALIASING "")

	option(ENABLE_ARCH_NATIVE "Optimize for this computer's CPU" OFF)
	add_feature_info("Non-portable optimizations (ENABLE_ARCH_NATIVE)" ENABLE_ARCH_NATIVE "")
endif()

option(DIST_BUILD "Build for stand-alone distribution")
add_feature_info("Distribution build (DIST_BUILD)" DIST_BUILD "")

option(DISABLE_UPDATE_CHECK_DEFAULT "Don't enable update checks by default" OFF)
add_feature_info("Update checking disabled by default (DISABLE_UPDATE_CHECK_DEFAULT)" DISABLE_UPDATE_CHECK_DEFAULT "")

if(NOT CMAKE_CROSSCOMPILING)
	if(DIST_BUILD)
		set(source_assets_default OFF)
	else()
		set(source_assets_default ON)
	endif()
	option(SOURCE_ASSETS "Take assets from source directory so that you don't have to install them (turn off for distribution!)" "${source_assets_default}")
	add_feature_info("Use assets from source directory (SOURCE_ASSETS)" SOURCE_ASSETS "")
endif()

cmake_dependent_option(PROXY_STYLE "" OFF "CLIENT" OFF)
add_feature_info(
	"Use proxy style to fix dark theme contrasts - only needed if not patching Qt, breaks on some non-English Windows versions (PROXY_STYLE)"
	PROXY_STYLE "")

set(AUDIO_IMPL MINIAUDIO CACHE STRING "Audio playback implementation (MINIAUDIO, QT)")
add_feature_info("Audio playback implementation (AUDIO_IMPL)" ON ${AUDIO_IMPL})

# Feature info will be emitted later once it is clear whether or not these
# features could actually be enabled
option(CLANG_TIDY "Automatically enable Clang-Tidy" OFF)
foreach(sanitizer IN ITEMS Address Leak Memory Thread UndefinedBehavior)
	if(sanitizer STREQUAL "UndefinedBehavior")
		set(san_upper UNDEFINED)
	else()
		string(TOUPPER "${sanitizer}" san_upper)
	endif()
	option(${san_upper}_SANITIZER "Automatically enable ${sanitizer}Sanitizer" OFF)
endforeach()
unset(san_upper)

add_feature_info("Unity build (CMAKE_UNITY_BUILD)" CMAKE_UNITY_BUILD "")
