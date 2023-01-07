find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Gui Sql)
find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} QUIET COMPONENTS LinguistTools)

add_library(dpclient STATIC
	document.cpp
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
	canvas/blendmodes.cpp
	canvas/canvasmodel.cpp
	canvas/canvasshortcuts.cpp
	canvas/documentmetadata.cpp
	canvas/indexbuilderrunnable.cpp
	canvas/inputpresetmodel.cpp
	canvas/layerlist.cpp
	canvas/paintengine.cpp
	canvas/selection.cpp
	canvas/timelinemodel.cpp
	canvas/userlist.cpp
	drawdance/aclstate.cpp
	drawdance/annotation.cpp
	drawdance/annotationlist.cpp
	drawdance/brushengine.cpp
	drawdance/brushpreview.cpp
	drawdance/canvashistory.cpp
	drawdance/canvasstate.cpp
	drawdance/documentmetadata.cpp
	drawdance/eventlog.cpp
	drawdance/frame.cpp
	drawdance/global.cpp
	drawdance/image.cpp
	drawdance/layercontent.cpp
	drawdance/layergroup.cpp
	drawdance/layerlist.cpp
	drawdance/layerprops.cpp
	drawdance/layerpropslist.cpp
	drawdance/message.cpp
	drawdance/paintengine.cpp
	drawdance/perf.cpp
	drawdance/snapshotqueue.cpp
	drawdance/tile.cpp
	drawdance/timeline.cpp
	drawdance/ziparchive.cpp
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
	tools/tool.cpp
	tools/toolcontroller.cpp
	tools/toolproperties.cpp
	tools/utils.cpp
	tools/zoom.cpp
	utils/avatarlistmodel.cpp
	utils/colorscheme.cpp
	utils/canvasshortcutsmodel.cpp
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
	brushes/brush.cpp
	brushes/brushpresetmodel.cpp
	export/videoexporter.cpp
	export/imageseriesexporter.cpp
	export/ffmpegexporter.cpp
	export/canvassaverrunnable.cpp
	export/animationsaverrunnable.cpp
	parentalcontrols/parentalcontrols.cpp
	parentalcontrols/parentalcontrols_$<IF:$<BOOL:${WIN32}>,win,dummy>.cpp
)

target_link_libraries(dpclient
	dpshared
	${QT_PACKAGE_NAME}::Gui
	${QT_PACKAGE_NAME}::Network
	${QT_PACKAGE_NAME}::Sql
	Drawdance::dpengine
	Drawdance::dpengine_archive
	Drawdance::dpmsg
	Drawdance::dpcommon
)

if(${QT_PACKAGE_NAME}LinguistTools_FOUND)
	set(ts_files
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
	target_sources(dpclient PRIVATE ${ts_files})
	set_source_files_properties(${ts_files} PROPERTIES
		OUTPUT_LOCATION "i18n"
	)
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/i18n")
	qt_add_translation(qm_files ${ts_files})
endif()
add_feature_info("Client translations" ${QT_PACKAGE_NAME}LinguistTools_FOUND "")

if(TESTS)
	add_subdirectory(tests)
endif()

target_auto_source_group(dpclient)
