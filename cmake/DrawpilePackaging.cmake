#[[ This module contains installer packaging settings. #]]

set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_THREADS 0)

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}")
if(BUILD_PACKAGE_COMPONENT)
	string(APPEND CPACK_PACKAGE_FILE_NAME "-${BUILD_PACKAGE_COMPONENT}")
endif()
string(APPEND CPACK_PACKAGE_FILE_NAME "-${PROJECT_VERSION}")
if(BUILD_PACKAGE_SUFFIX)
	string(APPEND CPACK_PACKAGE_FILE_NAME "-${BUILD_PACKAGE_SUFFIX}")
endif()

set(CPACK_PACKAGE_VENDOR "Drawpile contributors")
set(CPACK_PACKAGE_DESCRIPTION
	"Drawpile is a drawing program that lets you share the canvas with other "
	"users in real time."
)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A collaborative drawing program.")
set(CPACK_PACKAGE_ICON "${PROJECT_SOURCE_DIR}/pkg/installer.png")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_PACKAGE_EXECUTABLES "drawpile;Drawpile;drawpile-srv;Drawpile dedicated server")
if(APPLE)
	set(CPACK_MONOLITHIC_INSTALL TRUE)
	set(CPACK_GENERATOR DragNDrop)
	set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/pkg/installer.scpt")
	set(CPACK_DMG_BACKGROUND_IMAGE "${PROJECT_SOURCE_DIR}/pkg/background.png")
elseif(WIN32)
	# This is a monolithic install right now because `install(CODE)` does not
	# work with CPack
	set(CPACK_MONOLITHIC_INSTALL TRUE)
	set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
	if(CLIENT)
		set(CPACK_GENERATOR ZIP WIX)
		set(CPACK_WIX_UPGRADE_GUID DC47B534-E365-4054-85F0-2E7C6CCB76CC)
		set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/src/desktop/icons/drawpile.ico")
		set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT ${PROJECT_HOMEPAGE_URL})
		set(CPACK_WIX_PROPERTY_ARPURLUPDATEINFO ${PROJECT_HOMEPAGE_URL}/download/)
		set(CPACK_WIX_PROPERTY_ARPHELPLINK ${PROJECT_HOMEPAGE_URL}/help/)

		# CMake didn't set the install scope properly. They "fixed" this in
		# version 3.29. Unfortunately, this causes the installer to no longer
		# be compatible with previous installs, leading to updates becoming
		# impossible! It'll check if the application is already installed,
		# notice that the existing install is per-user while the new one is
		# per-machine and then MSI in all its wisdom will just install the
		# application halfway a second time and not replace any files, leading
		# to the user having multiple desktop shortcuts and start menu entries
		# and aaargh it hurts. I don't know if there's a way around this, but
		# for now, we're reverting to the old, now-deprecated behavior, because
		# that actually works. Yeah it won't create start menu shortcuts
		# properly if you have multiple users on your machine, but that's such
		# a niche use case that supporting it really doesn't matter. See
		# https://gitlab.kitware.com/cmake/cmake/-/issues/20962
		set(CPACK_WIX_INSTALL_SCOPE NONE)

		include(DrawpileFileExtensions)
		get_wix_extensions("${PROJECT_NAME}" "drawpile.exe" WIX_DRAWPILE_PROGIDS)
		if(MSVC)
			math(EXPR WIX_DRAWPILE_TOOLSET_VERSION_MAJOR "${MSVC_TOOLSET_VERSION} / 10")
		endif()

		if(CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(CPACK_WIX_ARCHITECTURE x86)
		else()
			set(CPACK_WIX_ARCHITECTURE x64)
		endif()
		message(STATUS "Configuring ${CPACK_WIX_ARCHITECTURE} installer")
		configure_file("pkg/installer-${CPACK_WIX_ARCHITECTURE}.wix.in" pkg/installer.wix)

		set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/pkg/installer.wix")
		if(HAVE_CODE_SIGNING)
			message(STATUS "Will sign installer executable")
			set(CPACK_POST_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/pkg/SignWindowsInstaller.cmake")
		endif()
	else()
		set(CPACK_GENERATOR ZIP)
	endif()
else()
	set(CPACK_COMPONENTS_ALL drawpile drawpile-srv tools i18n)
endif()

include(CPack)

cpack_add_component(i18n HIDDEN)
cpack_add_component(drawpile
	DISPLAY_NAME "Drawpile client"
	DESCRIPTION "The main Drawpile client application."
	DEPENDS i18n
)
cpack_add_component(drawpile-srv
	DISPLAY_NAME "Dedicated server"
	DESCRIPTION "A stand-alone dedicated server for hosting Drawpile sessions."
	DEPENDS i18n
)
cpack_add_component(tools
	DISPLAY_NAME "Command-line tools"
	DESCRIPTION "Tools for manipulating Drawpile recordings."
)
