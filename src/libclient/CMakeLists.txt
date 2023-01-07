find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Gui Sql)
find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} QUIET COMPONENTS LinguistTools)

add_library(dpclient STATIC
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
	canvas/timelinemodel.cpp
	canvas/documentmetadata.cpp
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
	parentalcontrols/parentalcontrols_$<IF:$<BOOL:${WIN32}>,win,dummy>.cpp
)

target_link_libraries(dpclient
	dpshared
	${QT_PACKAGE_NAME}::Gui
	${QT_PACKAGE_NAME}::Network
	${QT_PACKAGE_NAME}::Sql
	rustpile
	libmypaint
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
