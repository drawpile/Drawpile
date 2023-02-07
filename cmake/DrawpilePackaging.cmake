set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_THREADS 0)
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Drawpile contributors")
set(CPACK_PACKAGE_DESCRIPTION
	"Drawpile is a drawing program that lets you share the canvas with other "
	"users in real time."
)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A collaborative drawing program.")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/pkg/installer.png")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_PACKAGE_EXECUTABLES "drawpile;Drawpile;drawpile-srv;Drawpile dedicated server")
if(APPLE)
	set(CPACK_MONOLITHIC_INSTALL TRUE)
	set(CPACK_GENERATOR DragNDrop)
	set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/pkg/installer.scpt")
	set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/pkg/background.png")
elseif(WIN32)
	set(CPACK_MONOLITHIC_INSTALL TRUE)
	set(CPACK_GENERATOR ZIP WIX)
	set(CPACK_WIX_UPGRADE_GUID DC47B534-E365-4054-85F0-2E7C6CCB76CC)
	set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/desktop/assets/drawpile.ico")
	set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT ${CMAKE_PROJECT_HOMEPAGE_URL})
	set(CPACK_WIX_PROPERTY_ARPURLUPDATEINFO ${CMAKE_PROJECT_HOMEPAGE_URL}/download/)
	set(CPACK_WIX_PROPERTY_ARPHELPLINK ${CMAKE_PROJECT_HOMEPAGE_URL}/help/)
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

	include(DrawpileFileExtensions)
	get_wix_extensions("${PROJECT_NAME}" "drawpile.exe" WIX_DRAWPILE_PROGIDS)
	configure_file(pkg/installer.wix.in pkg/installer.wix)
	set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/pkg/installer.wix")
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
