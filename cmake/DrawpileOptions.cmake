#[[ This module defines all of the exposed build options. #]]

include(CMakeDependentOption)

option(CLIENT "Compile client" ON)
add_feature_info("Drawpile client (CLIENT)" CLIENT "")

# This does not really need to be reported as a feature
set(BUILD_LABEL "" CACHE STRING "A custom label to add to the version")

option(UPDATE_TRANSLATIONS "Update translation files from source")
add_feature_info(".ts regeneration (slow!) (UPDATE_TRANSLATIONS)" UPDATE_TRANSLATIONS "")

if(NOT ANDROID)
	option(SERVER "Compile dedicated server" OFF)
	add_feature_info("Drawpile server (SERVER)" SERVER "")

	cmake_dependent_option(SERVERGUI "Enable server GUI" ON "SERVER" OFF)
	add_feature_info("Server GUI (SERVERGUI)" SERVERGUI "")

	option(TOOLS "Compile extra tools" OFF)
	add_feature_info("Extra tools (TOOLS)" TOOLS "")

	option(TESTS "Build unit tests" OFF)
	add_feature_info("Unit tests (TESTS)" TESTS "")
else()
	# CMake allows unexposed options to be enabled
	set(SERVER OFF CACHE BOOL "" FORCE)
	set(SERVERGUI OFF CACHE BOOL "" FORCE)
	set(TOOLS OFF CACHE BOOL "" FORCE)
	set(TESTS OFF CACHE BOOL "" FORCE)
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

option(DIST_BUILD "Build for stand-alone distribution")
add_feature_info("Distribution build (DIST_BUILD)" DIST_BUILD "")

option(VERSION_CHECK "Enable code to check for updates" ON)
add_feature_info("Automatic update checking code (VERSION_CHECK)" VERSION_CHECK "")

# Feature info will be emitted later once it is clear whether or not these
# features could actually be enabled
option(CLANG_TIDY "Automatically enable Clang-Tidy" ON)
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
