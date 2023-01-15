set(desktop_sources
	main.cpp
	mainwindow.cpp
	notifications.cpp
	toolwidgets/toolsettings.cpp
	toolwidgets/brushsettings.cpp
	toolwidgets/colorpickersettings.cpp
	toolwidgets/selectionsettings.cpp
	toolwidgets/annotationsettings.cpp
	toolwidgets/fillsettings.cpp
	toolwidgets/lasersettings.cpp
	toolwidgets/zoomsettings.cpp
	toolwidgets/inspectorsettings.cpp
	scene/canvasscene.cpp
	scene/canvasview.cpp
	scene/canvasviewmodifiers.cpp
	scene/canvasitem.cpp
	scene/annotationitem.cpp
	scene/selectionitem.cpp
	scene/usermarkeritem.cpp
	scene/lasertrailitem.cpp
	dialogs/abusereport.cpp
	dialogs/certificateview.cpp
	dialogs/newdialog.cpp
	dialogs/hostdialog.cpp
	dialogs/joindialog.cpp
	dialogs/logindialog.cpp
	dialogs/settingsdialog.cpp
	dialogs/resizedialog.cpp
	dialogs/playbackdialog.cpp
	dialogs/videoexportdialog.cpp
	dialogs/netstats.cpp
	dialogs/flipbook.cpp
	dialogs/resetdialog.cpp
	dialogs/sessionsettings.cpp
	dialogs/serverlogdialog.cpp
	dialogs/tablettester.cpp
	dialogs/avatarimport.cpp
	dialogs/versioncheckdialog.cpp
	dialogs/addserverdialog.cpp
	dialogs/inputsettings.cpp
	dialogs/layerproperties.cpp
	dialogs/brushpresetproperties.cpp
	widgets/viewstatus.cpp
	widgets/popupmessage.cpp
	widgets/netstatus.cpp
	widgets/colorbutton.cpp
	widgets/brushpreview.cpp
	widgets/kis_curve_widget.cpp
	widgets/keysequenceedit.cpp
	widgets/groupedtoolbutton.cpp
	widgets/filmstrip.cpp
	widgets/resizerwidget.cpp
	widgets/tablettest.cpp
	widgets/flipbookview.cpp
	widgets/presetselector.cpp
	widgets/spinner.cpp
	widgets/modifierkeys.cpp
	widgets/notifbar.cpp
	widgets/KisAngleGauge.cpp
	widgets/timelinewidget.cpp
	docks/toolsettingsdock.cpp
	docks/brushpalettedock.cpp
	docks/navigator.cpp
	docks/colorspinner.cpp
	docks/colorsliders.cpp
	docks/colorpalette.cpp
	docks/layerlistdock.cpp
	docks/layerlistdelegate.cpp
	docks/layeraclmenu.cpp
	docks/onionskins.cpp
	docks/timeline.cpp
	docks/titlewidget.cpp
	chat/chatlineedit.cpp
	chat/chatwidgetpinnedarea.cpp
	chat/chatwidget.cpp
	chat/useritemdelegate.cpp
	chat/chatbox.cpp
	chat/chatwindow.cpp
	utils/mandatoryfields.cpp
	utils/recentfiles.cpp
	utils/listserverdelegate.cpp
	utils/hotbordereventfilter.cpp
)

set(desktop_uis
	ui/abusereport.ui
	ui/avatarimport.ui
	ui/brushdock.ui
	ui/brushpresetproperties.ui
	ui/certificateview.ui
	ui/fillsettings.ui
	ui/flipbook.ui
	ui/hostdialog.ui
	ui/inputcfg.ui
	ui/inspectorsettings.ui
	ui/joindialog.ui
	ui/lasersettings.ui
	ui/layerproperties.ui
	ui/logindialog.ui
	ui/netstats.ui
	ui/newdialog.ui
	ui/playback.ui
	ui/resetsession.ui
	ui/resizedialog.ui
	ui/selectsettings.ui
	ui/serverlog.ui
	ui/sessionsettings.ui
	ui/settings.ui
	ui/tablettest.ui
	ui/textsettings.ui
	ui/versioncheck.ui
	ui/videoexport.ui
)

set(bundled_qtcolorwidgets_sources
	bundled/QtColorWidgets/abstract_widget_list.cpp
	bundled/QtColorWidgets/bound_color_selector.cpp
	bundled/QtColorWidgets/color_2d_slider.cpp
	bundled/QtColorWidgets/color_delegate.cpp
	bundled/QtColorWidgets/color_dialog.cpp
	bundled/QtColorWidgets/color_line_edit.cpp
	bundled/QtColorWidgets/color_list_widget.cpp
	bundled/QtColorWidgets/color_names.cpp
	bundled/QtColorWidgets/color_palette.cpp
	bundled/QtColorWidgets/color_palette_model.cpp
	bundled/QtColorWidgets/color_palette_widget.cpp
	bundled/QtColorWidgets/color_preview.cpp
	bundled/QtColorWidgets/color_selector.cpp
	bundled/QtColorWidgets/color_utils.cpp
	bundled/QtColorWidgets/color_wheel.cpp
	bundled/QtColorWidgets/color_widgets.qrc
	bundled/QtColorWidgets/gradient_editor.cpp
	bundled/QtColorWidgets/gradient_list_model.cpp
	bundled/QtColorWidgets/gradient_slider.cpp
	bundled/QtColorWidgets/harmony_color_wheel.cpp
	bundled/QtColorWidgets/hue_slider.cpp
	bundled/QtColorWidgets/swatch.cpp
	bundled/QtColorWidgets/include/QtColorWidgets/AbstractWidgetList
	bundled/QtColorWidgets/include/QtColorWidgets/abstract_widget_list.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/BoundColorSelector
	bundled/QtColorWidgets/include/QtColorWidgets/bound_color_selector.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/CMakeLists.txt
	bundled/QtColorWidgets/include/QtColorWidgets/color_2d_slider.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/ColorDelegate
	bundled/QtColorWidgets/include/QtColorWidgets/color_delegate.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/ColorDialog
	bundled/QtColorWidgets/include/QtColorWidgets/color_dialog.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_line_edit.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/ColorListWidget
	bundled/QtColorWidgets/include/QtColorWidgets/color_list_widget.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_names.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_palette.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_palette_model.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_palette_widget.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/ColorPreview
	bundled/QtColorWidgets/include/QtColorWidgets/color_preview.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/ColorSelector
	bundled/QtColorWidgets/include/QtColorWidgets/color_selector.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_utils.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/ColorWheel
	bundled/QtColorWidgets/include/QtColorWidgets/color_wheel.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/color_wheel_private.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/colorwidgets_global.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/gradient_delegate.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/GradientEditor
	bundled/QtColorWidgets/include/QtColorWidgets/gradient_editor.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/gradient_helper.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/GradientListModel
	bundled/QtColorWidgets/include/QtColorWidgets/gradient_list_model.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/GradientSlider
	bundled/QtColorWidgets/include/QtColorWidgets/gradient_slider.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/HarmonyColorWheel
	bundled/QtColorWidgets/include/QtColorWidgets/harmony_color_wheel.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/HueSlider
	bundled/QtColorWidgets/include/QtColorWidgets/hue_slider.hpp
	bundled/QtColorWidgets/include/QtColorWidgets/swatch.hpp
)

add_executable(${CLIENTNAME} ${desktop_sources})
set_drawpile_target_properties(${CLIENTNAME})
target_include_directories("${CLIENTNAME}" PUBLIC
	"${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(${CLIENTNAME} PUBLIC dpclient
	Qt5::Widgets Qt5::Multimedia Qt5::Svg)

set_target_properties(${CLIENTNAME} PROPERTIES
	VERSION ${DRAWPILE_VERSION}
	PROJECT_LABEL drawpile-client)

if(WIN32 AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	# Don't show a console window in Windows in release mode.
	set_property(TARGET ${CLIENTNAME} PROPERTY WIN32_EXECUTABLE ON)
endif()

if(APPLE)
	set_property(TARGET ${CLIENTNAME} PROPERTY MACOSX_BUNDLE ON)
	target_sources(${CLIENTNAME} PRIVATE widgets/macmenu.cpp)
endif()

if(KIS_TABLET)
	if(WIN32)
		target_sources(${CLIENTNAME} PRIVATE
			bundled/kis_tablet/kis_tablet_support_win8.cpp
			bundled/kis_tablet/kis_tablet_support_win.cpp)
	elseif(NOT APPLE)
		target_sources(${CLIENTNAME} PRIVATE
			bundled/kis_tablet/kis_xi2_event_filter.cpp
			bundled/kis_tablet/qxcbconnection.cpp
			bundled/kis_tablet/qxcbconnection_xi2.cpp)
		target_link_libraries(${CLIENTNAME} PUBLIC
			Qt5::X11Extras ${X11_Xinput_LIB} ${XCB_LIBRARIES} ${X11_LIBRARIES})
	endif()
ENDIF()

if(QTKEYCHAIN_LIBRARIES)
	target_compile_definitions(${CLIENTNAME} PUBLIC HAVE_QTKEYCHAIN)
	target_link_libraries(${CLIENTNAME} PUBLIC ${QTKEYCHAIN_LIBRARIES})
else ()
	message(WARNING "QtKeyChain not found: password remembering not enabled")
endif()

# Use bundled QtColorWidgets widgets if library not found
if(QTCOLORWIDGETS_FOUND)
	target_include_directories(${CLIENTNAME} PUBLIC ${LIBQTCOLORWIDGETS_INCLUDE_DIRS})
	target_link_libraries(${CLIENTNAME} PUBLIC ${LIBQTCOLORWIDGETS_LIBRARIES})
else()
	message(WARNING "QtColorWidgets not found: using bundled copy!")
	qt5_add_resources(desktop_resources bundled/QtColorWidgets/color_widgets.qrc)
	qt5_wrap_ui(bundled_qtcolorwidgets_ui_headers
		bundled/QtColorWidgets/color_dialog.ui
		bundled/QtColorWidgets/color_palette_widget.ui)
	target_compile_definitions(${CLIENTNAME} PUBLIC QTCOLORPICKER_STATICALLY_LINKED)
	target_include_directories(${CLIENTNAME} PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/bundled/QtColorWidgets/include")
	target_sources(${CLIENTNAME} PRIVATE
		${bundled_qtcolorwidgets_sources} ${bundled_qtcolorwidgets_ui_headers})
endif()

qt5_add_resources(desktop_resources ui/cursors/cursors.qrc)
qt5_add_resources(desktop_resources ui/icons/builtin.qrc)
qt5_wrap_ui(desktop_ui_headers ${desktop_uis} )
target_sources(${CLIENTNAME} PRIVATE
	${desktop_ui_headers} ${desktop_resources} ${libclient_qm_translations})

if(WIN32)
	generate_win32_resource(
		Win32Resource
		"Drawpile client" # full name
		${CLIENTNAME} # internal name
		"Drawpile client"
		${DRAWPILE_HOMEPAGE_URL}
		"2006-2022 Calle Laakkonen"
		${PROJECT_VERSION}
		"${CMAKE_SOURCE_DIR}/desktop/dancepile.ico")
	target_sources(${CLIENTNAME} PRIVATE ${Win32Resource})
endif()

if(APPLE)
	set(MACOSX_BUNDLE_BUNDLE_NAME ${CLIENTNAME})
	# set how it shows up in the Info.plist file
	set(MACOSX_BUNDLE_ICON_FILE "drawpile.icns")
	# set the bundle identifier (REQUIRED, or some strange GUI bugs may appear)
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "net.drawpile.DrawpileClient")
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${DRAWPILE_VERSION}")
	set(MACOSX_BUNDLE_BUNDLE_VERSION "${DRAWPILE_VERSION}")
	set(MACOSX_BUNDLE_COPYRIGHT "Copyright Drawpile team")

	set(ResourcesDir "${CMAKE_BINARY_DIR}/bin/${CLIENTNAME}.app/Contents/Resources")
	add_custom_target(appbundle ALL
		COMMAND mkdir -p "${ResourcesDir}/i18n"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/drawpile.icns" "${ResourcesDir}"
		COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/../libclient/*.qm" "${ResourcesDir}/i18n"
		COMMAND mkdir -p "${ResourcesDir}/palettes"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/palettes/*.gpl" "${ResourcesDir}/palettes"
		COMMAND mkdir -p "${ResourcesDir}/sounds"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/sounds/*.wav" "${ResourcesDir}/sounds"
		COMMAND cp -r "${CMAKE_SOURCE_DIR}/desktop/theme" "${ResourcesDir}"
		COMMAND cp "${CMAKE_SOURCE_DIR}/desktop/initialbrushpresets.db" "${ResourcesDir}")
endif()

install(TARGETS ${CLIENTNAME} DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

if(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug|RelWithDebInfo")
	strip_exe(${CLIENTNAME})
endif()

