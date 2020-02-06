find_package(Qt5Gui REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5LinguistTools)
find_package(Vpx)

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
	canvas/statetracker.cpp
	canvas/canvasmodel.cpp
	canvas/selection.cpp
	canvas/usercursormodel.cpp
	canvas/lasertrailmodel.cpp
	canvas/retcon.cpp
	canvas/loader.cpp
	canvas/aclfilter.cpp
	canvas/userlist.cpp
	canvas/layerlist.cpp
	canvas/history.cpp
	canvas/canvassaverrunnable.cpp
	net/client.cpp
	net/server.cpp
	net/loopbackserver.cpp
	net/tcpserver.cpp
	net/login.cpp
	net/loginsessions.cpp
	net/sessionlistingmodel.cpp
	net/banlistmodel.cpp
	net/announcementlist.cpp
	net/commands.cpp
	utils/palette.cpp
	utils/palettelistmodel.cpp
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
	core/annotationmodel.cpp
	core/tile.cpp
	core/layer.cpp
	core/layerstack.cpp
	core/layerstackobserver.cpp
	core/layerstackpixmapcacheobserver.cpp
	core/brushmask.cpp
	core/blendmodes.cpp
	core/rasterop.cpp
	core/floodfill.cpp
	core/tilevector.cpp
	brushes/brush.cpp
	brushes/brushengine.cpp
	brushes/brushpainter.cpp
	brushes/classicbrushstate.cpp
	brushes/classicbrushpainter.cpp
	brushes/pixelbrushstate.cpp
	brushes/pixelbrushpainter.cpp
	brushes/shapes.cpp
	brushes/brushpresetmodel.cpp
	brushes/brushpresetmigration.cpp
	ora/orawriter.cpp
	ora/orareader.cpp
	recording/index.cpp
	recording/indexbuilder.cpp
	recording/indexloader.cpp
	recording/filter.cpp
	recording/playbackcontroller.cpp
	export/animation.cpp
	export/videoexporter.cpp
	export/imageseriesexporter.cpp
	parentalcontrols/parentalcontrols.cpp
)

include_directories(bundled)

if(WIN32)
	set(SOURCES ${SOURCES} parentalcontrols/parentalcontrols_win.cpp)
else()
	set(SOURCES ${SOURCES} parentalcontrols/parentalcontrols_dummy.cpp)
endif()

if(GIF_FOUND)
	set ( SOURCES ${SOURCES} export/gifexporter.cpp )
	add_definitions(-DHAVE_GIFLIB)
	include_directories(SYSTEM "${GIF_INCLUDE_DIR}")
endif()

if(LIBVPX_FOUND)
	set(
		SOURCES ${SOURCES}
		bundled/mkvmuxer/mkvmuxer.cc
		bundled/mkvmuxer/mkvmuxerutil.cc
		export/webmexporter.cpp
		export/webmencoder.cpp
	)
endif(LIBVPX_FOUND)

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

if(GIF_FOUND)
	target_link_libraries(dpclient ${GIF_LIBRARIES})
endif()

if(LIBVPX_FOUND)
	target_link_libraries(dpclient ${LIBVPX_LIBRARIES})
endif(LIBVPX_FOUND)

if ( WIN32 )
	target_link_libraries (dpclient ws2_32)
endif ()

if( QM_TRANSLATIONS )
	install( FILES ${QM_TRANSLATIONS} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile/i18n" )
endif( QM_TRANSLATIONS)

if(TESTS)
    add_subdirectory(tests)
endif(TESTS)

