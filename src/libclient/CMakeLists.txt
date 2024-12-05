find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Gui)

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_library(dpclient)

target_sources(dpclient PRIVATE
	document.cpp
	document.h
	settings_table.h
	settings.cpp
	settings.h
	brushes/brush.cpp
	brushes/brush.h
	brushes/brushpresetmodel.cpp
	brushes/brushpresetmodel.h
	canvas/acl.cpp
	canvas/acl.h
	canvas/blendmodes.cpp
	canvas/blendmodes.h
	canvas/canvasmodel.cpp
	canvas/canvasmodel.h
	canvas/canvasshortcuts.cpp
	canvas/canvasshortcuts.h
	canvas/documentmetadata.cpp
	canvas/documentmetadata.h
	canvas/indexbuilderrunnable.cpp
	canvas/indexbuilderrunnable.h
	canvas/layerlist.cpp
	canvas/layerlist.h
	canvas/paintengine.cpp
	canvas/paintengine.h
	canvas/point.h
	canvas/selectionmodel.cpp
	canvas/selectionmodel.h
	canvas/tilecache.cpp
	canvas/tilecache.h
	canvas/timelinemodel.cpp
	canvas/timelinemodel.h
	canvas/transformmodel.cpp
	canvas/transformmodel.h
	canvas/userlist.cpp
	canvas/userlist.h
	parentalcontrols/parentalcontrols.cpp
	parentalcontrols/parentalcontrols.h
	drawdance/aclstate.cpp
	drawdance/aclstate.h
	drawdance/annotation.cpp
	drawdance/annotation.h
	drawdance/annotationlist.cpp
	drawdance/annotationlist.h
	drawdance/brushengine.cpp
	drawdance/brushengine.h
	drawdance/brushpreview.cpp
	drawdance/brushpreview.h
	drawdance/canvashistory.cpp
	drawdance/canvashistory.h
	drawdance/canvasstate.cpp
	drawdance/canvasstate.h
	drawdance/documentmetadata.cpp
	drawdance/documentmetadata.h
	drawdance/eventlog.cpp
	drawdance/eventlog.h
	drawdance/global.cpp
	drawdance/global.h
	drawdance/image.cpp
	drawdance/image.h
	drawdance/keyframe.cpp
	drawdance/keyframe.h
	drawdance/layercontent.cpp
	drawdance/layercontent.h
	drawdance/layergroup.cpp
	drawdance/layergroup.h
	drawdance/layerlist.cpp
	drawdance/layerlist.h
	drawdance/layerprops.cpp
	drawdance/layerprops.h
	drawdance/layerpropslist.cpp
	drawdance/layerpropslist.h
	drawdance/paintengine.cpp
	drawdance/paintengine.h
	drawdance/perf.cpp
	drawdance/perf.h
	drawdance/selection.cpp
	drawdance/selection.h
	drawdance/selectionset.cpp
	drawdance/selectionset.h
	drawdance/snapshotqueue.cpp
	drawdance/snapshotqueue.h
	drawdance/tile.cpp
	drawdance/tile.h
	drawdance/timeline.cpp
	drawdance/timeline.h
	drawdance/track.cpp
	drawdance/track.h
	drawdance/viewmode.cpp
	drawdance/viewmode.h
	drawdance/ziparchive.cpp
	drawdance/ziparchive.h
	export/animationformat.cpp
	export/animationformat.h
	export/animationsaverrunnable.cpp
	export/animationsaverrunnable.h
	export/canvassaverrunnable.cpp
	export/canvassaverrunnable.h
	import/animationimporter.cpp
	import/animationimporter.h
	import/canvasloaderrunnable.cpp
	import/canvasloaderrunnable.h
	import/loadresult.cpp
	import/loadresult.h
	net/announcementlist.cpp
	net/announcementlist.h
	net/authlistmodel.cpp
	net/authlistmodel.h
	net/banlistmodel.cpp
	net/banlistmodel.h
	net/client.cpp
	net/client.h
	net/login.cpp
	net/login.h
	net/loginsessions.cpp
	net/loginsessions.h
	net/message.cpp
	net/message.h
	net/server.cpp
	net/server.h
	net/sessionlistingmodel.cpp
	net/sessionlistingmodel.h
	tools/annotation.cpp
	tools/annotation.h
	tools/beziertool.cpp
	tools/beziertool.h
	tools/clickdetector.h
	tools/colorpicker.cpp
	tools/colorpicker.h
	tools/enums.h
	tools/floodfill.cpp
	tools/floodfill.h
	tools/freehand.cpp
	tools/freehand.h
	tools/inspector.cpp
	tools/inspector.h
	tools/laser.cpp
	tools/laser.h
	tools/magicwand.cpp
	tools/magicwand.h
	tools/pan.cpp
	tools/pan.h
	tools/selection.cpp
	tools/selection.h
	tools/shapetools.cpp
	tools/shapetools.h
	tools/tool.cpp
	tools/tool.h
	tools/toolcontroller.cpp
	tools/toolcontroller.h
	tools/toolproperties.cpp
	tools/toolproperties.h
	tools/toolstate.h
	tools/transform.cpp
	tools/transform.h
	tools/utils.cpp
	tools/utils.h
	tools/zoom.cpp
	tools/zoom.h
	utils/annotations.cpp
	utils/annotations.h
	utils/avatarlistmodel.cpp
	utils/avatarlistmodel.h
	utils/avatarlistmodeldelegate.h
	utils/brushshortcutmodel.cpp
	utils/brushshortcutmodel.h
	utils/canvasshortcutsmodel.cpp
	utils/canvasshortcutsmodel.h
	utils/certificatestoremodel.cpp
	utils/certificatestoremodel.h
	utils/changeflags.h
	utils/colorscheme.cpp
	utils/colorscheme.h
	utils/cursors.cpp
	utils/cursors.h
	utils/customshortcutmodel.cpp
	utils/customshortcutmodel.h
	utils/debouncetimer.cpp
	utils/debouncetimer.h
	utils/funstuff.cpp
	utils/funstuff.h
	utils/hostpresetmodel.cpp
	utils/hostpresetmodel.h
	utils/html.cpp
	utils/html.h
	utils/html_gen.cpp
	utils/identicon.cpp
	utils/identicon.h
	utils/images.cpp
	utils/images.h
	utils/keyframelayermodel.cpp
	utils/keyframelayermodel.h
	utils/kis_cubic_curve.cpp
	utils/kis_cubic_curve.h
	utils/listservermodel.cpp
	utils/listservermodel.h
	utils/logging.cpp
	utils/logging.h
	utils/scopedoverridecursor.h
	utils/selectionalteration.cpp
	utils/selectionalteration.h
	utils/selectionoutlinegenerator.cpp
	utils/selectionoutlinegenerator.h
	utils/sessionfilterproxymodel.cpp
	utils/sessionfilterproxymodel.h
	utils/sessionidvalidator.cpp
	utils/sessionidvalidator.h
	utils/shortcutdetector.cpp
	utils/shortcutdetector.h
	utils/statedatabase.cpp
	utils/statedatabase.h
	utils/transformquad.h
	utils/usernamevalidator.cpp
	utils/usernamevalidator.h
	utils/wasmpersistence.h
)

if(ANDROID OR EMSCRIPTEN)
	# Android doesn't allow images to be placed on the system clipboard.
	# Emscripten can't freely access it. Emulate it via an internal buffer.
	target_compile_definitions(dpclient PUBLIC HAVE_CLIPBOARD_EMULATION=1)
else()
	target_sources(dpclient PRIVATE
		export/ffmpegexporter.cpp
		export/ffmpegexporter.h
		export/imageseriesexporter.cpp
		export/imageseriesexporter.h
		export/videoexporter.cpp
		export/videoexporter.h
	)
endif()

if(EMSCRIPTEN)
	target_sources(dpclient PRIVATE
		wasmsupport.cpp
		wasmsupport.h
		utils/wasmpersistence.cpp
	)
else()
	target_sources(dpclient PRIVATE utils/news.cpp utils/news.h)
endif()

if(HAVE_TCPSOCKETS)
	target_sources(dpclient PRIVATE
		net/tcpserver.cpp
		net/tcpserver.h
	)
	if(BUILTINSERVER)
		target_compile_definitions(dpclient PUBLIC DP_HAVE_BUILTIN_SERVER=1)
		target_link_libraries(dpclient PUBLIC dpserver)
		target_sources(dpclient PRIVATE
			server/builtinclient.cpp
			server/builtinclient.h
			server/builtinsession.cpp
			server/builtinsession.h
			server/builtinserver.cpp
			server/builtinserver.h
		)
	endif()
endif()

# https://gitlab.kitware.com/cmake/cmake/-/issues/19813
if(WIN32)
	target_sources(dpclient PRIVATE
		parentalcontrols/parentalcontrols_win.cpp
	)
else()
	target_sources(dpclient PRIVATE
		parentalcontrols/parentalcontrols_dummy.cpp
	)
endif()

target_link_libraries(dpclient
	PRIVATE
		cmake-config
	PUBLIC
		dpshared
		drawdance_client
		${QT_PACKAGE_NAME}::Core
		${QT_PACKAGE_NAME}::Network
		${QT_PACKAGE_NAME}::Gui
)

if(TARGET ${QT_PACKAGE_NAME}::WebSockets)
	target_sources(dpclient PRIVATE
		net/websocketserver.cpp
		net/websocketserver.h
	)
	target_link_libraries(dpclient PUBLIC ${QT_PACKAGE_NAME}::WebSockets)
endif()

if(${QT_PACKAGE_NAME}LinguistTools_FOUND)
	add_subdirectory(i18n)
endif()

if(TESTS)
	add_subdirectory(tests)
endif()

directory_auto_source_groups()
