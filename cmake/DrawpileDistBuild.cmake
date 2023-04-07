#[[
This module sets up the helpers that are required to create distributable
builds.
#]]

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
	# This just simplifies environment handling later by making sure it is never
	# empty since passing variables into `install(CODE)` sucks
	set(extra_env "bogus=")
elseif(WIN32)
	set(helper_name windeployqt)
	# Required translation files are already copied by install
	set(helper_flags "--release;--no-translations;--pdb;--no-system-d3d-compiler;--no-compiler-runtime")
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
		message(STATUS "(\"qt.conf already exists\" warnings are normal.)")
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
