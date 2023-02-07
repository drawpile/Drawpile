find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Widgets Multimedia)
find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} QUIET COMPONENTS LinguistTools)
find_package(QtColorWidgets QUIET)
find_package(${QT_PACKAGE_NAME}Keychain QUIET)

add_executable(drawpile MACOSX_BUNDLE
	filewrangler.cpp
	filewrangler.h
	main.cpp
	main.h
	mainwindow.cpp
	mainwindow.h
	notifications.cpp
	notifications.h
	tabletinput.cpp
	tabletinput.h
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
	cursors/arrow.png
	cursors/bucket.png
	cursors/colorpicker.png
	cursors/curve.png
	cursors/dpcursors.qrc
	cursors/ellipse.png
	cursors/layerpicker.png
	cursors/line.png
	cursors/rectangle.png
	cursors/rotate.png
	cursors/select-lasso.png
	cursors/select-rectangle.png
	cursors/text.png
	cursors/triangle-left.png
	cursors/triangle-right.png
	cursors/zoom.png
	dialogs/abusereport.cpp
	dialogs/abusereport.h
	dialogs/addserverdialog.cpp
	dialogs/addserverdialog.h
	dialogs/avatarimport.cpp
	dialogs/avatarimport.h
	dialogs/brushpresetproperties.cpp
	dialogs/brushpresetproperties.h
	dialogs/brushsettingsdialog_gen.cpp
	dialogs/brushsettingsdialog.cpp
	dialogs/brushsettingsdialog.h
	dialogs/canvasshortcutsdialog.cpp
	dialogs/canvasshortcutsdialog.h
	dialogs/certificateview.cpp
	dialogs/certificateview.h
	dialogs/colordialog.cpp
	dialogs/colordialog.h
	dialogs/dumpplaybackdialog.cpp
	dialogs/dumpplaybackdialog.h
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
	dialogs/layoutsdialog.cpp
	dialogs/layoutsdialog.h
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
	docks/onionskins.cpp
	docks/onionskins.h
	docks/timeline.cpp
	docks/timeline.h
	docks/titlewidget.cpp
	docks/titlewidget.h
	docks/toolsettingsdock.cpp
	docks/toolsettingsdock.h
	icons/builtin.qrc
	icons/censored.svg
	icons/drawpile.ico
	icons/drawpile.iconset/icon_512x512@2x.png
	icons/drawpile.png
	icons/drawpile.svg
	scene/annotationitem.cpp
	scene/annotationitem.h
	scene/arrows_data.h
	scene/canvasitem.cpp
	scene/canvasitem.h
	scene/canvasscene.cpp
	scene/canvasscene.h
	scene/canvasview.cpp
	scene/canvasview.h
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
	icons/builtin.qrc
	icons/censored.svg
	icons/dancepile.png
	icons/drawpile.ico
	icons/drawpile.iconset/icon_512x512@2x.png
	icons/drawpile.png
	icons/drawpile.svg
	ui/abusereport.ui
	ui/avatarimport.ui
	ui/brushdock.ui
	ui/brushpresetproperties.ui
	ui/canvasshortcutsdialog.ui
	ui/certificateview.ui
	ui/dumpplayback.ui
	ui/fillsettings.ui
	ui/flipbook.ui
	ui/hostdialog.ui
	ui/inputcfg.ui
	ui/inspectorsettings.ui
	ui/joindialog.ui
	ui/lasersettings.ui
	ui/layerproperties.ui
	ui/layoutsdialog.ui
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
	utils/hidedocktitlebarseventfilter.cpp
	utils/hidedocktitlebarseventfilter.h
	utils/listserverdelegate.cpp
	utils/listserverdelegate.h
	utils/mandatoryfields.cpp
	utils/mandatoryfields.h
	utils/recentfiles.cpp
	utils/recentfiles.h
	utils/widgetutils.h
	widgets/brushpreview.cpp
	widgets/brushpreview.h
	widgets/canvasshortcutedit.cpp
	widgets/canvasshortcutedit.h
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
	widgets/kis_double_parse_spin_box.cpp
	widgets/kis_double_parse_spin_box.h
	widgets/kis_helpers.h
	widgets/kis_int_parse_spin_box.cpp
	widgets/kis_int_parse_spin_box.h
	widgets/kis_num_parser.cpp
	widgets/kis_num_parser.h
	widgets/kis_parse_spin_box_p.h
	widgets/kis_slider_spin_box_p.h
	widgets/kis_slider_spin_box.cpp
	widgets/kis_slider_spin_box.h
	widgets/KisAngleGauge.cpp
	widgets/KisAngleGauge.h
	widgets/macmenu.cpp
	widgets/macmenu.h
	widgets/mypaintinput.cpp
	widgets/mypaintinput.h
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
	widgets/toolmessage.cpp
	widgets/toolmessage.h
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

# On Qt6, private headers are required to switch between Windows Ink and Wintab.
if(WIN32 AND QT_VERSION_MAJOR VERSION_EQUAL 6)
	# TODO: Check this is doing the right thing
	target_include_directories(drawpile PRIVATE "${Qt6Gui_PRIVATE_INCLUDE_DIRS}")
endif()

# Android doesn't sensibly support multiple windows, they just cover each other
# up with no way to switch between them, so we don't allow that there.
if(ANDROID)
	target_compile_definitions(drawpile PRIVATE SINGLE_MAIN_WINDOW=1)
	target_sources(drawpile PRIVATE
		dialogs/androidfiledialog.cpp
		dialogs/androidfiledialog.h
		ui/androidfiledialog.ui
	)
endif()

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
	target_link_libraries(drawpile ${QT_PACKAGE_NAME}Keychain::${QT_PACKAGE_NAME}Keychain)
endif()
add_feature_info("Client password manager" ${QT_PACKAGE_NAME}Keychain_FOUND "")

if(QtColorWidgets_FOUND)
	target_link_libraries(drawpile QtColorWidgets::QtColorWidgets)
else()
	target_sources(drawpile PRIVATE
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
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
		MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_GIT_REVISION}
		MACOSX_BUNDLE_COPYRIGHT "Copyright © Drawpile contributors"
		MACOSX_BUNDLE_GUI_IDENTIFIER net.drawpile.DrawpileClient
		MACOSX_BUNDLE_ICON_FILE drawpile.icns
		MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME} ${PROJECT_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
		MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
	)

	include(DrawpileFileExtensions)
	get_plist_extensions(drawpile.icns MACOSX_BUNDLE_EXTENSIONS)

	add_custom_command(
		OUTPUT drawpile.icns
		DEPENDS icons/drawpile.iconset/icon_512x512@2x.png
		COMMAND iconutil -c icns -o drawpile.icns "${CMAKE_CURRENT_SOURCE_DIR}/icons/drawpile.iconset"
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
	COMPONENT drawpile
)

function(install_datarootdir target files destination)
	install(
		FILES ${files}
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${destination}"
		COMPONENT ${target}
		${ARGN}
	)
endfunction()

if(UNIX AND NOT APPLE)
	include(DrawpileFileExtensions)

	get_xdg_extensions(MIME_TYPES)
	configure_file(drawpile.desktop.in drawpile.desktop)
	install_datarootdir(drawpile "${CMAKE_CURRENT_BINARY_DIR}/drawpile.desktop" applications
		RENAME net.drawpile.drawpile.desktop
	)
	install_datarootdir(drawpile icons/drawpile.png icons/hicolor/256x256/apps)
	install_datarootdir(drawpile icons/drawpile.svg icons/hicolor/scalable/apps)
	install_datarootdir(drawpile appdata.xml metainfo
		RENAME net.drawpile.drawpile.appdata.xml
	)

	generate_xdg_mime_info("${CMAKE_CURRENT_BINARY_DIR}/mime")
	install(
		DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mime"
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}"
		COMPONENT drawpile
	)
	install_datarootdir(drawpile icons/drawpile.png
		icons/hicolor/256x256/mimetypes
		RENAME application-vnd.drawpile.recording.png
	)
	install_datarootdir(drawpile icons/drawpile.svg
		icons/hicolor/scalable/mimetypes
		RENAME application-vnd.drawpile.recording.svg
	)
	install_datarootdir(drawpile icons/drawpile.png
		icons/hicolor/256x256/mimetypes
		RENAME text-vnd.drawpile.recording.png
	)
	install_datarootdir(drawpile icons/drawpile.svg
		icons/hicolor/scalable/mimetypes
		RENAME text-vnd.drawpile.recording.svg
	)
endif()

target_auto_source_group(drawpile)
