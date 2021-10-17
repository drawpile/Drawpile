find_package(Qt5Gui REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5LinguistTools)

set (
	SOURCES
	document.cpp
	tools/toolcontroller.cpp
	tools/toolproperties.cpp
	tools/utils.cpp
	tools/annotation.cpp
	tools/freehand.cpp
	tools/colorpicker.cpp
	tools/laser.cpp
	tools/selection.cpp
	tools/shapetools.cpp
	tools/beziertool.cpp
	tools/floodfill.cpp
	tools/strokesmoother.cpp
	tools/zoom.cpp
	tools/inspector.cpp
	canvas/canvasmodel.cpp
	canvas/selection.cpp
	canvas/acl.cpp
	canvas/userlist.cpp
	canvas/layerlist.cpp
	canvas/inputpresetmodel.cpp
	canvas/paintengine.cpp
	canvas/indexbuilderrunnable.cpp
	canvas/blendmodes.cpp
	net/client.cpp
	net/server.cpp
	net/tcpserver.cpp
	net/login.cpp
	net/loginsessions.cpp
	net/sessionlistingmodel.cpp
	net/banlistmodel.cpp
	net/announcementlist.cpp
	net/envelopebuilder.cpp
	net/messagequeue.cpp
	net/servercmd.cpp
	utils/html.cpp
	utils/usernamevalidator.cpp
	utils/sessionidvalidator.cpp
	utils/images.cpp
	utils/funstuff.cpp
	utils/kis_cubic_curve.cpp
	utils/shortcutdetector.cpp
	utils/customshortcutmodel.cpp
	utils/listservermodel.cpp
	utils/colorscheme.cpp
	utils/icon.cpp
	utils/logging.cpp
	utils/identicon.cpp
	utils/avatarlistmodel.cpp
	utils/sessionfilterproxymodel.cpp
	utils/newversion.cpp
	brushes/brush.cpp
	brushes/brushpresetmodel.cpp
	export/videoexporter.cpp
	export/imageseriesexporter.cpp
	export/ffmpegexporter.cpp
	export/canvassaverrunnable.cpp
	export/animationsaverrunnable.cpp
	parentalcontrols/parentalcontrols.cpp
)

include_directories(bundled)

if(WIN32)
	set(SOURCES ${SOURCES} parentalcontrols/parentalcontrols_win.cpp)
else()
	set(SOURCES ${SOURCES} parentalcontrols/parentalcontrols_dummy.cpp)
endif()

if( Qt5LinguistTools_FOUND)
	set(TRANSLATIONS
		i18n/drawpile_fi.ts
		i18n/drawpile_cs.ts
		i18n/drawpile_ja.ts
		i18n/drawpile_ru.ts
		i18n/drawpile_de.ts
		i18n/drawpile_zh.ts
		i18n/drawpile_vi.ts
		i18n/drawpile_uk.ts
		i18n/drawpile_it.ts
		i18n/drawpile_fr.ts
		i18n/drawpile_pt.ts
	)

	qt5_add_translation(QM_TRANSLATIONS ${TRANSLATIONS})
endif()

add_library (
	dpclient
	STATIC
	${SOURCES}
	${QM_TRANSLATIONS}
)

target_link_libraries(dpclient dpshared Qt5::Gui Qt5::Network)
target_link_libraries(dpclient rustpile pthread dl)

if ( WIN32 )
	target_link_libraries (dpclient ws2_32 userenv)
endif ()

if( QM_TRANSLATIONS )
	install( FILES ${QM_TRANSLATIONS} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile/i18n" )
endif( QM_TRANSLATIONS)

if(TESTS)
    add_subdirectory(tests)
endif(TESTS)

