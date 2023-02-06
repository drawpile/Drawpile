find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Widgets Multimedia)
find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} QUIET COMPONENTS LinguistTools)
find_package(QtColorWidgets QUIET)
find_package(${QT_PACKAGE_NAME}Keychain QUIET)

add_executable(drawpile MACOSX_BUNDLE
	main.cpp
	main.h
	mainwindow.cpp
	mainwindow.h
	notifications.cpp
	notifications.h
	chat/chatbox.cpp
	chat/chatbox.h
	chat/chatlineedit.cpp
	chat/chatlineedit.h
	chat/chatwidget.cpp
	chat/chatwidget.h
	chat/chatwidgetpinnedarea.cpp
	chat/chatwidgetpinnedarea.h
	chat/chatwindow.cpp
	chat/chatwindow.h
	chat/useritemdelegate.cpp
	chat/useritemdelegate.h
	dialogs/abusereport.cpp
	dialogs/abusereport.h
	dialogs/addserverdialog.cpp
	dialogs/addserverdialog.h
	dialogs/avatarimport.cpp
	dialogs/avatarimport.h
	dialogs/brushpresetproperties.cpp
	dialogs/brushpresetproperties.h
	dialogs/certificateview.cpp
	dialogs/certificateview.h
	dialogs/flipbook.cpp
	dialogs/flipbook.h
	dialogs/hostdialog.cpp
	dialogs/hostdialog.h
	dialogs/inputsettings.cpp
	dialogs/inputsettings.h
	dialogs/joindialog.cpp
	dialogs/joindialog.h
	dialogs/layerproperties.cpp
	dialogs/layerproperties.h
	dialogs/logindialog.cpp
	dialogs/logindialog.h
	dialogs/netstats.cpp
	dialogs/netstats.h
	dialogs/newdialog.cpp
	dialogs/newdialog.h
	dialogs/playbackdialog.cpp
	dialogs/playbackdialog.h
	dialogs/resetdialog.cpp
	dialogs/resetdialog.h
	dialogs/resizedialog.cpp
	dialogs/resizedialog.h
	dialogs/serverlogdialog.cpp
	dialogs/serverlogdialog.h
	dialogs/sessionsettings.cpp
	dialogs/sessionsettings.h
	dialogs/settingsdialog.cpp
	dialogs/settingsdialog.h
	dialogs/tablettester.cpp
	dialogs/tablettester.h
	dialogs/versioncheckdialog.cpp
	dialogs/versioncheckdialog.h
	dialogs/videoexportdialog.cpp
	dialogs/videoexportdialog.h
	docks/brushpalettedock.cpp
	docks/brushpalettedock.h
	docks/colorpalette.cpp
	docks/colorpalette.h
	docks/colorsliders.cpp
	docks/colorsliders.h
	docks/colorspinner.cpp
	docks/colorspinner.h
	docks/layeraclmenu.cpp
	docks/layeraclmenu.h
	docks/layerlistdelegate.cpp
	docks/layerlistdelegate.h
	docks/layerlistdock.cpp
	docks/layerlistdock.h
	docks/navigator.cpp
	docks/navigator.h
	docks/timeline.cpp
	docks/timeline.h
	docks/titlewidget.cpp
	docks/titlewidget.h
	docks/toolsettingsdock.cpp
	docks/toolsettingsdock.h
	scene/annotationitem.cpp
	scene/annotationitem.h
	scene/arrows_data.h
	scene/canvasitem.cpp
	scene/canvasitem.h
	scene/canvasscene.cpp
	scene/canvasscene.h
	scene/canvasview.cpp
	scene/canvasview.h
	scene/canvasviewmodifiers.cpp
	scene/canvasviewmodifiers.h
	scene/lasertrailitem.cpp
	scene/lasertrailitem.h
	scene/selectionitem.cpp
	scene/selectionitem.h
	scene/usermarkeritem.cpp
	scene/usermarkeritem.h
	toolwidgets/annotationsettings.cpp
	toolwidgets/annotationsettings.h
	toolwidgets/brushsettings.cpp
	toolwidgets/brushsettings.h
	toolwidgets/colorpickersettings.cpp
	toolwidgets/colorpickersettings.h
	toolwidgets/fillsettings.cpp
	toolwidgets/fillsettings.h
	toolwidgets/inspectorsettings.cpp
	toolwidgets/inspectorsettings.h
	toolwidgets/lasersettings.cpp
	toolwidgets/lasersettings.h
	toolwidgets/selectionsettings.cpp
	toolwidgets/selectionsettings.h
	toolwidgets/toolsettings.cpp
	toolwidgets/toolsettings.h
	toolwidgets/zoomsettings.cpp
	toolwidgets/zoomsettings.h
	ui/cursors/arrow.png
	ui/cursors/bucket.png
	ui/cursors/colorpicker.png
	ui/cursors/cursors.qrc
	ui/cursors/curve.png
	ui/cursors/ellipse.png
	ui/cursors/layerpicker.png
	ui/cursors/line.png
	ui/cursors/rectangle.png
	ui/cursors/rotate.png
	ui/cursors/select-lasso.png
	ui/cursors/select-rectangle.png
	ui/cursors/text.png
	ui/cursors/zoom.png
	ui/icons/builtin.qrc
	ui/icons/censored.svg
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
	utils/actionbuilder.h
	utils/hotbordereventfilter.cpp
	utils/hotbordereventfilter.h
	utils/listserverdelegate.cpp
	utils/listserverdelegate.h
	utils/mandatoryfields.cpp
	utils/mandatoryfields.h
	utils/recentfiles.cpp
	utils/recentfiles.h
	widgets/brushpreview.cpp
	widgets/brushpreview.h
	widgets/colorbutton.cpp
	widgets/colorbutton.h
	widgets/filmstrip.cpp
	widgets/filmstrip.h
	widgets/flipbookview.cpp
	widgets/flipbookview.h
	widgets/groupedtoolbutton.cpp
	widgets/groupedtoolbutton.h
	widgets/keysequenceedit.cpp
	widgets/keysequenceedit.h
	widgets/kis_curve_widget_p.h
	widgets/kis_curve_widget.cpp
	widgets/kis_curve_widget.h
	widgets/KisAngleGauge.cpp
	widgets/KisAngleGauge.h
	widgets/modifierkeys.cpp
	widgets/modifierkeys.h
	widgets/netstatus.cpp
	widgets/netstatus.h
	widgets/notifbar.cpp
	widgets/notifbar.h
	widgets/popupmessage.cpp
	widgets/popupmessage.h
	widgets/presetselector.cpp
	widgets/presetselector.h
	widgets/resizerwidget.cpp
	widgets/resizerwidget.h
	widgets/spinner.cpp
	widgets/spinner.h
	widgets/tablettest.cpp
	widgets/tablettest.h
	widgets/timelinewidget.cpp
	widgets/timelinewidget.h
	widgets/viewstatus.cpp
	widgets/viewstatus.h
)

# https://gitlab.kitware.com/cmake/cmake/-/issues/19813
if(APPLE)
	target_sources(drawpile PRIVATE
		widgets/macmenu.cpp
		widgets/macmenu.h
	)
endif()

if(${QT_PACKAGE_NAME}LinguistTools_FOUND)
	add_subdirectory(i18n)
endif()

set_target_properties(drawpile PROPERTIES
	AUTOUIC_SEARCH_PATHS ui
	VERSION ${PROJECT_VERSION}
	PROJECT_LABEL drawpile-client
	WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
)

target_link_libraries(drawpile
	dpclient
	${QT_PACKAGE_NAME}::Widgets
	${QT_PACKAGE_NAME}::Multimedia
)

if(KIS_TABLET)
	target_sources(drawpile PRIVATE
		bundled/kis_tablet/debug.h
		bundled/kis_tablet/kis_tablet_support_win_p.h
		bundled/kis_tablet/kis_tablet_support_win.cpp
		bundled/kis_tablet/kis_tablet_support_win.h
		bundled/kis_tablet/kis_tablet_support_win8.cpp
		bundled/kis_tablet/kis_tablet_support_win8.h
		bundled/kis_tablet/pktdef.h
		bundled/kis_tablet/wintab.h
	)
endif()

if(${QT_PACKAGE_NAME}Keychain_FOUND)
	target_compile_definitions(drawpile PRIVATE HAVE_QTKEYCHAIN)
	if(TARGET ${QT_PACKAGE_NAME}Keychain::${QT_PACKAGE_NAME}Keychain)
		target_link_libraries(drawpile ${QT_PACKAGE_NAME}Keychain::${QT_PACKAGE_NAME}Keychain)
	else()
		# QtKeychain < 0.13
		target_link_libraries(drawpile ${QTKEYCHAIN_LIBRARIES})
	endif()
endif()
add_feature_info("Client password manager" ${QT_PACKAGE_NAME}Keychain_FOUND "")

if(QtColorWidgets_FOUND)
	target_link_libraries(drawpile QtColorWidgets::QtColorWidgets)
else()
	set(sources
		bundled/QtColorWidgets/include/QtColorWidgets/abstract_widget_list.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/AbstractWidgetList
		bundled/QtColorWidgets/include/QtColorWidgets/bound_color_selector.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/BoundColorSelector
		bundled/QtColorWidgets/include/QtColorWidgets/color_2d_slider.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_delegate.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_dialog.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_line_edit.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_list_widget.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_names.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_palette_model.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_palette_widget.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_palette.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_preview.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_selector.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_utils.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_wheel_private.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/color_wheel.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/ColorDelegate
		bundled/QtColorWidgets/include/QtColorWidgets/ColorDialog
		bundled/QtColorWidgets/include/QtColorWidgets/ColorListWidget
		bundled/QtColorWidgets/include/QtColorWidgets/ColorPreview
		bundled/QtColorWidgets/include/QtColorWidgets/ColorSelector
		bundled/QtColorWidgets/include/QtColorWidgets/ColorWheel
		bundled/QtColorWidgets/include/QtColorWidgets/colorwidgets_global.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/gradient_delegate.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/gradient_editor.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/gradient_helper.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/gradient_list_model.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/gradient_slider.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/GradientEditor
		bundled/QtColorWidgets/include/QtColorWidgets/GradientListModel
		bundled/QtColorWidgets/include/QtColorWidgets/GradientSlider
		bundled/QtColorWidgets/include/QtColorWidgets/harmony_color_wheel.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/HarmonyColorWheel
		bundled/QtColorWidgets/include/QtColorWidgets/hue_slider.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/HueSlider
		bundled/QtColorWidgets/include/QtColorWidgets/qt_compatibility.hpp
		bundled/QtColorWidgets/include/QtColorWidgets/swatch.hpp
		bundled/QtColorWidgets/abstract_widget_list.cpp
		bundled/QtColorWidgets/alphaback.png
		bundled/QtColorWidgets/bound_color_selector.cpp
		bundled/QtColorWidgets/color_2d_slider.cpp
		bundled/QtColorWidgets/color_delegate.cpp
		bundled/QtColorWidgets/color_dialog.cpp
		bundled/QtColorWidgets/color_dialog.ui
		bundled/QtColorWidgets/color_line_edit.cpp
		bundled/QtColorWidgets/color_list_widget.cpp
		bundled/QtColorWidgets/color_names.cpp
		bundled/QtColorWidgets/color_palette_model.cpp
		bundled/QtColorWidgets/color_palette_widget.cpp
		bundled/QtColorWidgets/color_palette_widget.ui
		bundled/QtColorWidgets/color_palette.cpp
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
	)
	target_sources(drawpile PRIVATE ${sources})
	if(MSVC)
		set_source_files_properties(${sources} PROPERTIES
			COMPILE_OPTIONS "/W0"
		)
	else()
		set_source_files_properties(${sources} PROPERTIES
			COMPILE_OPTIONS "-Wno-all;-Wno-extra;-Wno-error"
		)
	endif()
	target_include_directories(drawpile SYSTEM PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/bundled/QtColorWidgets/include"
	)
	target_compile_definitions(drawpile PRIVATE QTCOLORPICKER_STATICALLY_LINKED)
endif()
add_feature_info("External QtColorWidgets" QTCOLORWIDGETS_FOUND "")

if(WIN32)
	configure_file(drawpile.rc.in drawpile.rc)
	target_sources(drawpile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/drawpile.rc")
elseif(APPLE)
	set_target_properties(drawpile PROPERTIES
		RUNTIME_OUTPUT_NAME Drawpile
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/DrawpileInfo.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
		MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_GIT_REVISION}
		MACOSX_BUNDLE_COPYRIGHT "Copyright © Drawpile contributors"
		MACOSX_BUNDLE_GUI_IDENTIFIER net.drawpile.DrawpileClient
		MACOSX_BUNDLE_ICON_FILE drawpile.icns
		MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME} ${PROJECT_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
		MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
	)

	add_custom_command(
		OUTPUT drawpile.icns
		DEPENDS assets/drawpile.iconset/icon_512x512@2x.png
		COMMAND iconutil -c icns -o drawpile.icns "${CMAKE_CURRENT_SOURCE_DIR}/assets/drawpile.iconset"
	)
	target_sources(drawpile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/drawpile.icns")
	set_property(TARGET drawpile APPEND PROPERTY RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/drawpile.icns")
endif()

include(AddResources)
add_resources(drawpile
	FILES initialbrushpresets.db
	DIRS palettes sounds theme
)

install(TARGETS drawpile
	BUNDLE DESTINATION "${INSTALL_BUNDLEDIR}"
	RESOURCE DESTINATION "${INSTALL_APPDATADIR}"
)

if(UNIX AND NOT APPLE)
	install(FILES assets/net.drawpile.drawpile.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
	install(FILES assets/drawpile.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps)
	install(FILES assets/drawpile.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
	install(FILES assets/drawpile.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/mimetypes RENAME application-x-drawpile-recording.png)
	install(FILES assets/drawpile.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/mimetypes RENAME application-x-drawpile-recording.svg)
	install(FILES assets/net.drawpile.drawpile.appdata.xml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
	install(FILES assets/x-drawpile.xml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
endif()

target_auto_source_group(drawpile)
