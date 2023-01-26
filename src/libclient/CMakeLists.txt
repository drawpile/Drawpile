set(libclient_sources
	brushes/brush.cpp
	brushes/brushpresetmodel.cpp
	canvas/acl.cpp
	canvas/blendmodes.cpp
	canvas/canvasmodel.cpp
	canvas/documentmetadata.cpp
	canvas/indexbuilderrunnable.cpp
	canvas/inputpresetmodel.cpp
	canvas/layerlist.cpp
	canvas/paintengine.cpp
	canvas/selection.cpp
	canvas/timelinemodel.cpp
	canvas/userlist.cpp
	document.cpp
	drawdance/aclstate.cpp
	drawdance/annotation.cpp
	drawdance/annotationlist.cpp
	drawdance/brushengine.cpp
	drawdance/brushpreview.cpp
	drawdance/canvashistory.cpp
	drawdance/canvasstate.cpp
	drawdance/drawcontextpool.cpp
	drawdance/documentmetadata.cpp
	drawdance/frame.cpp
	drawdance/image.cpp
	drawdance/perf.cpp
	drawdance/layercontent.cpp
	drawdance/layergroup.cpp
	drawdance/layerlist.cpp
	drawdance/layerprops.cpp
	drawdance/layerpropslist.cpp
	drawdance/logging.cpp
	drawdance/message.cpp
	drawdance/paintengine.cpp
	drawdance/snapshotqueue.cpp
	drawdance/timeline.cpp
	drawdance/tile.cpp
	export/animationsaverrunnable.cpp
	export/canvassaverrunnable.cpp
	export/ffmpegexporter.cpp
	export/imageseriesexporter.cpp
	export/videoexporter.cpp
	net/announcementlist.cpp
	net/banlistmodel.cpp
	net/client.cpp
	net/login.cpp
	net/loginsessions.cpp
	net/messagequeue.cpp
	net/server.cpp
	net/servercmd.cpp
	net/sessionlistingmodel.cpp
	net/tcpserver.cpp
	parentalcontrols/parentalcontrols.cpp
	tools/annotation.cpp
	tools/beziertool.cpp
	tools/colorpicker.cpp
	tools/floodfill.cpp
	tools/freehand.cpp
	tools/inspector.cpp
	tools/laser.cpp
	tools/selection.cpp
	tools/shapetools.cpp
	tools/strokesmoother.cpp
	tools/toolcontroller.cpp
	tools/toolproperties.cpp
	tools/utils.cpp
	tools/zoom.cpp
	utils/avatarlistmodel.cpp
	utils/colorscheme.cpp
	utils/customshortcutmodel.cpp
	utils/funstuff.cpp
	utils/html.cpp
	utils/icon.cpp
	utils/identicon.cpp
	utils/images.cpp
	utils/kis_cubic_curve.cpp
	utils/listservermodel.cpp
	utils/logging.cpp
	utils/newversion.cpp
	utils/sessionfilterproxymodel.cpp
	utils/sessionidvalidator.cpp
	utils/shortcutdetector.cpp
	utils/usernamevalidator.cpp
)

set(libclient_tss
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

add_library(dpclient STATIC ${libclient_sources})
set_drawpile_target_properties(dpclient)
target_include_directories(dpclient PUBLIC "${CMAKE_CURRENT_LIST_DIR}" bundled)
target_link_libraries(dpclient PUBLIC
	dpshared Drawdance::dpengine Drawdance::dpengine_archive Drawdance::dpmsg
	Drawdance::dpcommon Qt5::Gui Qt5::Network Qt5::Sql)

if(WIN32)
	target_sources(dpclient PRIVATE parentalcontrols/parentalcontrols_win.cpp)
	target_link_libraries(dpclient PUBLIC ws2_32 userenv bcrypt)
else()
	target_sources(dpclient PRIVATE parentalcontrols/parentalcontrols_dummy.cpp)
endif()

if(Qt5LinguistTools_FOUND)
	qt5_add_translation(libclient_qm_translations ${libclient_tss})
	target_sources(dpclient PRIVATE ${libclient_qm_translations})
	install(FILES ${libclient_qm_translations}
		DESTINATION "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile/i18n")
endif()

if(TESTS)
    add_subdirectory(tests)
endif()

