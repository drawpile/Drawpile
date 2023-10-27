#[[
This module sets up appropriate installation directories for each supported
target OS.
#]]

# This just makes it easier to access generated binaries while debugging instead
# of having to dig around deep inside the build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

if(DIST_BUILD AND UNIX AND NOT APPLE AND NOT ANDROID)
	# linuxdeploy wants everything in a usr subdirectory, so force
	# GNUInstallDirs to generate those paths by temporarily lying about what
	# CMAKE_INSTALL_PREFIX is set to
	set(OLD_CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
	set(CMAKE_INSTALL_PREFIX "/")
	include(GNUInstallDirs)
	set(CMAKE_INSTALL_PREFIX ${OLD_CMAKE_INSTALL_PREFIX})
else()
	include(GNUInstallDirs)
endif()

# GNUInstallDirs uses `PROJECT_NAME` verbatim for `DOCDIR`, but that makes the
# directory wrongly capitalised, so override it
string(TOLOWER "${PROJECT_NAME}" lower_project_name)
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${lower_project_name}")
unset(lower_project_name)

if(APPLE)
	if(CMAKE_INSTALL_PREFIX STREQUAL "/")
		set(INSTALL_BUNDLEDIR Applications CACHE STRING "macOS application bundle installation directory")
	else()
		set(INSTALL_BUNDLEDIR . CACHE STRING "macOS application bundle installation directory")
	endif()
	# Cannot use `TARGET_BUNDLE_CONTENT_DIR` since that points to the binary
	# output directory, not the install directory
	set(INSTALL_APPBUNDLEDIR "${INSTALL_BUNDLEDIR}/$<TARGET_PROPERTY:drawpile,RUNTIME_OUTPUT_NAME>.app")
	set(INSTALL_APPCONTENTSDIR "${INSTALL_APPBUNDLEDIR}/Contents")
	set(INSTALL_APPDATADIR "${INSTALL_APPCONTENTSDIR}/Resources")

	# If client is built then put everything into the bundle, otherwise put
	# things in the standard *nix places. Another option would be to put them
	# in the DMG in a separate directory that users could optionally copy, but
	# that is more complicated and no one likes spending time writing build
	# scripts.
	if(CLIENT)
		set(CMAKE_INSTALL_BINDIR "${INSTALL_APPCONTENTSDIR}/MacOS")
		set(CMAKE_INSTALL_MANDIR "${INSTALL_APPDATADIR}/man")
		set(CMAKE_INSTALL_DOCDIR "${INSTALL_APPDATADIR}/doc")
	endif()
elseif(WIN32)
	set(CMAKE_INSTALL_BINDIR .)
	set(INSTALL_APPDATADIR "${CMAKE_INSTALL_BINDIR}/data")
else()
	# Qt combines the organization name and the application name, so the path
	# for application data contains this redundancy
	set(INSTALL_APPDATADIR "${CMAKE_INSTALL_DATAROOTDIR}/drawpile/drawpile")
endif()

# Installs an executable with various quirks for the current platform.
# * On Windows, we want to dump the DLLs into the install directory. Except in
#   our distribution build, because CI already gathers this stuff differently.
# * On macOS, take care of MACOSX_BUNDLE stuff, whatever that is.
# * On Android, don't install anything, because executables are libraries.
# * On Linux, nothing special happens.
function(dp_install_executables)
	cmake_parse_arguments(PARSE_ARGV 0 ARG "INCLUDE_BUNDLEDIR" "" "TARGETS")
	if(WIN32 AND NOT DIST_BUILD)
		install(TARGETS ${ARG_TARGETS} RUNTIME DESTINATION ".")
	elseif(APPLE AND ARG_INCLUDE_BUNDLEDIR)
		install(TARGETS ${ARG_TARGETS} BUNDLE DESTINATION "${INSTALL_BUNDLEDIR}")
	elseif(NOT ANDROID)
		install(TARGETS ${ARG_TARGETS})
	endif()
endfunction()
