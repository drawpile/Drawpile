include(ECMInstallIcons)

ecm_install_icons(ICONS
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/16-apps-drawpile.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/32-apps-drawpile.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/48-apps-drawpile.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/64-apps-drawpile.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/128-apps-drawpile.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/256-apps-drawpile.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/apps/sc-apps-drawpile.svgz"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/16-mimetypes-application-x-drawpile-recording.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/32-mimetypes-application-x-drawpile-recording.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/48-mimetypes-application-x-drawpile-recording.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/64-mimetypes-application-x-drawpile-recording.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/128-mimetypes-application-x-drawpile-recording.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/256-mimetypes-application-x-drawpile-recording.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/hicolor/mimetypes/sc-mimetypes-application-x-drawpile-recording.svgz"
	DESTINATION "${KDE_INSTALL_ICONDIR}" THEME hicolor)

install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/net.drawpile.drawpile.desktop" DESTINATION "${XDG_APPS_INSTALL_DIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/net.drawpile.drawpile.appdata.xml" DESTINATION "${KDE_INSTALL_METAINFODIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/x-drawpile.xml" DESTINATION "${XDG_MIME_INSTALL_DIR}")

set(DPDATADIR "${DATA_INSTALL_DIR}/drawpile/drawpile")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/palettes" DESTINATION "${DPDATADIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sounds" DESTINATION "${DPDATADIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/theme" DESTINATION "${DPDATADIR}")
install(FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/nightmode.colors"
	"${CMAKE_CURRENT_SOURCE_DIR}/kritabright.colors"
	"${CMAKE_CURRENT_SOURCE_DIR}/kritadark.colors"
	"${CMAKE_CURRENT_SOURCE_DIR}/kritadarker.colors"
	"${CMAKE_CURRENT_SOURCE_DIR}/initialbrushpresets.db"
	DESTINATION "${DPDATADIR}")

