cmake_minimum_required(VERSION 3.18)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "macOS deployment target")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. Please specify a"
	" build directory when running CMake"
	" (`${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B <dir_name>`).\n"
	"You should also delete these files, which have already been generated:\n"
	"* ${CMAKE_BINARY_DIR}/CMakeCache.txt\n"
	"* ${CMAKE_BINARY_DIR}/CMakeFiles")
endif()

include(FeatureSummary)
include(DrawpileOptions)
include(DrawpileVersions)

project(Drawpile
	VERSION ${PROJECT_VERSION}
	HOMEPAGE_URL https://drawpile.net
)

# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html
find_package(QT NAMES Qt6 Qt5 REQUIRED)
message(STATUS "Using Qt version ${QT_VERSION_MAJOR}")
set(QT_PACKAGE_NAME Qt${QT_VERSION_MAJOR})

# There is an inherent conflict that the docs say `CMAKE_OSX_DEPLOYMENT_TARGET`
# needs to be set before calling `enable_language` or `project`, but it is
# impossible to run `find_package` before calling one of those, so instead we
# default it to the absolute minimum above, and then increase it to whatever Qt
# requires here
include(QtMacDeploymentTarget)
set_mac_deployment_target(${QT_VERSION})

# This check must happen after `project()` because `CMAKE_CONFIGURATION_TYPES`
# is not populated by defaults until then
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(FATAL_ERROR "Required build type missing."
		" Re-run CMake and specify one of these build types:\n"
		"* -DCMAKE_BUILD_TYPE=Debug\n"
		"* -DCMAKE_BUILD_TYPE=Release\n"
		"* -DCMAKE_BUILD_TYPE=RelWithDebInfo\n"
		"* -DCMAKE_BUILD_TYPE=MinSizeRel")
endif()

# CMake does not allow labels on the version in the project command, but having
# the split version number is useful for other places, so just pretend like it
# does semver and maybe it will someday
set(PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_LABEL}")

message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Protocol version: ${DRAWPILE_PROTO_SERVER_VERSION}.${DRAWPILE_PROTO_MAJOR_VERSION}.${DRAWPILE_PROTO_MINOR_VERSION}")
message(STATUS "Git revision: ${PROJECT_GIT_REVISION}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP "Generated Files")
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER "Generated Targets")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(DrawpileCompileOptions)

include(AutoSourceGroup)

if(TESTS)
	enable_testing()
	include(Tests)
endif()

# linuxdeploy wants everything in a usr subdirectory, so force GNUInstallDirs
# to generate those paths
if(DIST_BUILD AND UNIX AND NOT APPLE)
	set(OLD_CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
	set(CMAKE_INSTALL_PREFIX "/")
	include(GNUInstallDirs)
	set(CMAKE_INSTALL_PREFIX ${OLD_CMAKE_INSTALL_PREFIX})
else()
	include(GNUInstallDirs)
endif()

# GNUInstallDirs uses the PROJECT_NAME verbatim, but then the case is wrong, so
# just override that
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/drawpile")
include(Cargo)

if(APPLE)
	if(CMAKE_INSTALL_PREFIX STREQUAL "/")
		set(INSTALL_BUNDLEDIR Applications CACHE STRING "macOS application bundle installation directory")
	else()
		set(INSTALL_BUNDLEDIR . CACHE STRING "macOS application bundle installation directory")
	endif()
	# Cannot use TARGET_BUNDLE_CONTENT_DIR since that points to the binary
	# output directory, not the install directory.
	set(INSTALL_APPBUNDLEDIR "${INSTALL_BUNDLEDIR}/$<TARGET_PROPERTY:drawpile,RUNTIME_OUTPUT_NAME>.app")
	set(INSTALL_APPCONTENTSDIR "${INSTALL_APPBUNDLEDIR}/Contents")
	set(INSTALL_APPDATADIR "${INSTALL_APPCONTENTSDIR}/Resources")
	# If client is built then put everything into the bundle, otherwise put
	# things in the standard *nix places. Another option would be to put them
	# in the DMG in a separate directory that users could optionally copy.
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

if(CLIENT OR SERVER)
	include_directories(${CMAKE_SOURCE_DIR}/src)

	find_package(${QT_PACKAGE_NAME} ${QT_MIN_VERSION_SERVER} REQUIRED COMPONENTS Core Network)
	find_package(libsodium QUIET)
	find_package(Drawdance REQUIRED COMPONENTS dpcommon dpengine dpmsg)

	if(libsodium_FOUND)
		add_compile_definitions(HAVE_LIBSODIUM)
	endif()
	add_feature_info("Ext-auth support" libsodium_FOUND "")

	#find_package(KF5DNSSD NO_MODULE) # disabled pending built-in server rewrite
	if(KF5DNSSD_FOUND)
		add_compile_definitions(HAVE_DNSSD)
		# KF5DNSSD 5.84.0 moves a bunch of headers to a different place.
		if("${KF5DNSSD_VERSION}" VERSION_LESS "5.84.0")
			add_compile_definitions(HAVE_DNSSD_BEFORE_5_84_0)
		endif()
	endif()

	message(STATUS "Generating config.h")
	include(DrawpileFileExtensions)
	get_qt_extensions(RECORDING DRAWPILE_FILE_GROUP_RECORDING)
	get_qt_extensions(IMAGE DRAWPILE_FILE_GROUP_IMAGE)
	configure_file(src/config.h.in config/config.h)
	include_directories(${CMAKE_CURRENT_BINARY_DIR}/config)

	message(STATUS "Adding libshared")
	add_subdirectory(src/libshared)
endif()

if(CLIENT)
	message(STATUS "Adding libclient")
	add_subdirectory(src/libclient)

	message(STATUS "Adding desktop")
	add_subdirectory(src/desktop)
endif()

if(SERVER)
	message(STATUS "Adding libserver")
	add_subdirectory(src/libserver)

	message(STATUS "Adding thinsrv")
	add_subdirectory(src/thinsrv)
endif()

if(TOOLS)
	message(STATUS "Someone deleted the tools")
endif()

function (get_all_targets out_var current_dir)
	get_property(targets DIRECTORY ${current_dir} PROPERTY BUILDSYSTEM_TARGETS)
	get_property(subdirs DIRECTORY ${current_dir} PROPERTY SUBDIRECTORIES)

	foreach(subdir ${subdirs})
		get_all_targets(subdir_targets ${subdir})
		list(APPEND targets ${subdir_targets})
	endforeach()

	set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

get_all_targets(ALL_TARGETS "${CMAKE_CURRENT_LIST_DIR}")
foreach(target IN LISTS ALL_TARGETS)
	get_target_property(has_automoc ${target} AUTOMOC)
	get_target_property(binary_dir ${target} BINARY_DIR)
	if(has_automoc)
		set_source_files_properties(
			"${binary_dir}/${target}_autogen/mocs_compilation.cpp"
			TARGET_DIRECTORY "${target}"
			PROPERTIES
			COMPILE_OPTIONS "${IGNORE_WARNINGS_COMPILE_OPTIONS}"
		)
	endif()
endforeach()

if(DIST_BUILD)
	if(APPLE)
		set(helper_name macdeployqt)
		set(helper_flags "-no-strip")
		set(path_flags "")
		set(extra_exe_flag "-executable=")
		set(app_path "${INSTALL_APPBUNDLEDIR}")
		set(qt_conf_path "${INSTALL_APPDATADIR}")
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
			"[Paths]\n"
			"Plugins = PlugIns\n"
			"Translations = ${INSTALL_APPDATADIR}/i18n\n"
		)
		set(extra_env "bogus=")
	elseif(WIN32)
		set(helper_name windeployqt)
		set(helper_flags "--release;--no-translations;--pdb")
		set(path_flags "")
		set(extra_exe_flag "")
		set(app_path "${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:drawpile>")
		set(qt_conf_path "${CMAKE_INSTALL_BINDIR}")
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
			"[Paths]\n"
			"Translations = ${INSTALL_APPDATADIR}/i18n\n"
		)
		set(lib_paths "")
		foreach(path IN LISTS CMAKE_PREFIX_PATH)
			list(APPEND lib_paths "${path}/bin")
		endforeach()
		set(extra_env "PATH=${lib_paths}$ENV{PATH}")
		# Prevent CMake string parsing errors treating backslashes as
		# escape sequences when transferring variables into the install
		# script
		string(REPLACE "\\" "/" extra_env "${extra_env}")
	else()
		set(helper_name linuxdeploy-x86_64.AppImage)
		set(helper_flags "--plugin;qt;--output;appimage")
		set(path_flags "--appdir")
		set(extra_exe_flag "--executable=")
		set(app_path "")
		set(qt_conf_path "")
		set(lib_paths "")
		foreach(path IN LISTS CMAKE_PREFIX_PATH)
			if(lib_paths)
				set(lib_paths "${lib_paths}:")
			endif()
			set(lib_paths "${lib_paths}${path}/lib")
		endforeach()
		set(extra_env "LD_LIBRARY_PATH=${lib_paths}")
	endif()

	if(SERVER)
		list(APPEND helper_flags "${extra_exe_flag}\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:drawpile-srv>")
	endif()

	if(TOOLS AND NOT WIN32)
		list(APPEND helper_flags "${extra_exe_flag}\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:drawpile-cli>")
	endif()

	if((SERVER OR TOOLS) AND UNIX AND NOT APPLE)
		configure_file(
			pkg/custom-apprun.sh.in
			pkg/custom-apprun.sh
			@ONLY
		)
		list(APPEND helper_flags "--custom-apprun;${CMAKE_CURRENT_BINARY_DIR}/pkg/custom-apprun.sh")
	endif()

	# qt.conf and translations are handled automatically by
	# linuxdeploy-plugin-qt
	if(qt_conf_path)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
			DESTINATION "${qt_conf_path}"
			COMPONENT fixup
		)
		if(CLIENT)
			include(DrawpileTranslations)
			install_qt_translations(${INSTALL_APPDATADIR}/i18n
				qt qtbase qtmultimedia
			)
		endif()
	endif()

	if(TARGET ${QT_PACKAGE_NAME}::${helper_name})
		get_target_property(deployqt ${QT_PACKAGE_NAME}::${helper_name} LOCATION)
	endif()
	if(NOT deployqt)
		find_program(deployqt NAMES ${helper_name})
	endif()
	if(NOT deployqt)
		message(FATAL_ERROR "Could not find ${helper_name}")
	endif()

	install(CODE "
		set(deployqt \"${deployqt}\")
		set(helper_name \"${helper_name}\")
		set(helper_flags \"${helper_flags}\")
		set(app_path \"${app_path}\")
		set(path_flags \"${path_flags}\")
		set(extra_env \"${extra_env}\")
		set(INSTALL_APPBUNDLEDIR \"${INSTALL_APPBUNDLEDIR}\")
	" COMPONENT fixup)
	install(CODE [[
		message(STATUS "Running ${helper_name}...")
		if(APPLE)
			message(STATUS "(Unexpected prefix \"@executable_path\" errors are normal.)")
		endif()
		execute_process(
			COMMAND "${CMAKE_COMMAND}" -E env "${extra_env}"
				"${deployqt}"
				${path_flags}
				"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${app_path}"
				${helper_flags}
			RESULT_VARIABLE result
			COMMAND_ECHO STDOUT
		)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR "Deployment tool failed")
		endif()
	]] COMPONENT fixup)
endif()

feature_summary(INCLUDE_QUIET_PACKAGES
	WHAT PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)

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

include(DrawpilePackaging)
