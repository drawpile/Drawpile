find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5LinguistTools)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(KF5DNSSD NO_MODULE)
find_package(GIF)
find_package(Miniupnpc)

set (
	SOURCES
	document.cpp
	tools/toolcontroller.cpp
	tools/toolproperties.cpp
	tools/utils.cpp
	tools/annotation.cpp
	tools/brushes.cpp
	tools/colorpicker.cpp
	tools/laser.cpp
	tools/selection.cpp
	tools/shapetools.cpp
	tools/floodfill.cpp
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
	net/client.cpp
	net/server.cpp
	net/loopbackserver.cpp
	net/tcpserver.cpp
	net/login.cpp
	net/loginsessions.cpp
	net/serverthread.cpp
	net/builtinserver.cpp
	net/sessionlistingmodel.cpp
	net/banlistmodel.cpp
	net/commands.cpp
	utils/archive.cpp
	utils/palette.cpp
	utils/palettelistmodel.cpp
	utils/strokesmoother.cpp
	utils/html.cpp
	utils/usernamevalidator.cpp
	utils/sessionidvalidator.cpp
	utils/images.cpp
	utils/funstuff.cpp
	utils/kis_cubic_curve.cpp
	utils/shortcutdetector.cpp
	utils/customshortcutmodel.cpp
	utils/listservermodel.cpp
	utils/settings.cpp
	utils/icon.cpp
	core/annotationmodel.cpp
	core/tile.cpp
	core/layer.cpp
	core/layerstack.cpp
	core/brush.cpp
	core/brushmask.cpp
	core/blendmodes.cpp
	core/rasterop.cpp
	core/shapes.cpp
	core/floodfill.cpp
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
	export/ffmpegexporter.cpp
	parentalcontrols/parentalcontrols.cpp
)

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

if(LIBMINIUPNPC_FOUND)
	set ( SOURCES ${SOURCES} net/upnp.cpp )
	add_definitions(-DHAVE_UPNP)
endif()

if(KF5DNSSD_FOUND) 
	set(
		SOURCES ${SOURCES}
		net/serverdiscoverymodel.cpp
	)
	add_definitions(-DHAVE_DNSSD)
endif()

if( Qt5LinguistTools_FOUND)
	set(TRANSLATIONS
		i18n/drawpile_fi.ts
		i18n/drawpile_cs.ts
		i18n/drawpile_ja.ts
		i18n/drawpile_ru.ts
		i18n/drawpile_de.ts
		i18n/drawpile_zh.ts
	)

	qt5_add_translation(QM_TRANSLATIONS ${TRANSLATIONS})
endif()

add_library (
	${DPCLIENTLIB}
	STATIC
	${SOURCES}
	${QM_TRANSLATIONS}
)

target_link_libraries(${DPCLIENTLIB} ${DPSHAREDLIB} Qt5::Gui Qt5::Network Qt5::Concurrent)

if(KF5DNSSD_FOUND) 
	target_link_libraries(${DPCLIENTLIB} KF5::DNSSD)
endif() 

if(GIF_FOUND)
	target_link_libraries(${DPCLIENTLIB} ${GIF_LIBRARIES})
endif()

if(LIBMINIUPNPC_FOUND)
	target_link_libraries(${DPCLIENTLIB} ${LIBMINIUPNPC_LIBRARIES})
	if ( WIN32 )
		target_link_libraries (${DPCLIENTLIB} iphlpapi)
	endif ( WIN32 )
endif()

if ( WIN32 )
	target_link_libraries (${DPCLIENTLIB} ws2_32)
endif ()

if( QM_TRANSLATIONS )
	install( FILES ${QM_TRANSLATIONS} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile/i18n" )
endif( QM_TRANSLATIONS)

