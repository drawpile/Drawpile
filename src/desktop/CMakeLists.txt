find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Gui OpenGL Sql Svg Widgets Multimedia)
if(QT_VERSION VERSION_GREATER_EQUAL 6)
	find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS OpenGLWidgets)
endif()
find_package(QtColorWidgets QUIET)
find_package(${QT_PACKAGE_NAME}Keychain QUIET)

set(gui_identifier net.drawpile.DrawpileClient)

if(ANDROID)
	if(QT_VERSION VERSION_GREATER_EQUAL 6)
		qt_add_executable(drawpile)
	else()
		add_library(drawpile MODULE)
		add_dependencies(apk drawpile)
	endif()
elseif(EMSCRIPTEN)
	if(QT_VERSION VERSION_GREATER_EQUAL 6)
		qt_add_executable(drawpile)
	else()
		message(FATAL_ERROR "Building Emscripten on Qt5 is not supported")
	endif()
else()
	add_executable(drawpile MACOSX_BUNDLE)
endif()

dp_sign_executable(drawpile)
set_target_properties(drawpile PROPERTIES
	AUTOUIC_SEARCH_PATHS ui
	PROJECT_LABEL drawpile-client
	WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
)

target_sources(drawpile PRIVATE
	filewrangler.cpp
	filewrangler.h
	main.cpp
	main.h
	mainwindow.cpp
	mainwindow.h
	notifications.cpp
	notifications.h
	settings_table.h
	settings.cpp
	settings.h
	tabletinput.cpp
	tabletinput.h
	chat/chatbox.cpp
	chat/chatbox.h
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
	cursors/eraser.png
	cursors/layerpicker.png
	cursors/line.png
	cursors/rectangle.png
	cursors/rotate.png
	cursors/rotate-discrete.png
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
	dialogs/animationimportdialog.cpp
	dialogs/animationimportdialog.h
	dialogs/avatarimport.cpp
	dialogs/avatarimport.h
	dialogs/brushexportdialog.cpp
	dialogs/brushexportdialog.h
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
	dialogs/curvepresetdialog.cpp
	dialogs/curvepresetdialog.h
	dialogs/dumpplaybackdialog.cpp
	dialogs/dumpplaybackdialog.h
	dialogs/filetypedialog.cpp
	dialogs/filetypedialog.h
	dialogs/flipbook.cpp
	dialogs/flipbook.h
	dialogs/invitedialog.cpp
	dialogs/invitedialog.h
	dialogs/keyframepropertiesdialog.cpp
	dialogs/keyframepropertiesdialog.h
	dialogs/layerproperties.cpp
	dialogs/layerproperties.h
	dialogs/layoutsdialog.cpp
	dialogs/layoutsdialog.h
	dialogs/logindialog.cpp
	dialogs/logindialog.h
	dialogs/netstats.cpp
	dialogs/netstats.h
	dialogs/playbackdialog.cpp
	dialogs/playbackdialog.h
	dialogs/resetdialog.cpp
	dialogs/resetdialog.h
	dialogs/resizedialog.cpp
	dialogs/resizedialog.h
	dialogs/serverlogdialog.cpp
	dialogs/serverlogdialog.h
	dialogs/sessionundodepthlimitdialog.cpp
	dialogs/sessionundodepthlimitdialog.h
	dialogs/sessionsettings.cpp
	dialogs/sessionsettings.h
	dialogs/settingsdialog.cpp
	dialogs/settingsdialog.h
	dialogs/settingsdialog/canvasshortcuts.cpp
	dialogs/settingsdialog/canvasshortcuts.h
	dialogs/settingsdialog/general.cpp
	dialogs/settingsdialog/general.h
	dialogs/settingsdialog/helpers.h
	dialogs/settingsdialog/input.cpp
	dialogs/settingsdialog/input.h
	dialogs/settingsdialog/network.cpp
	dialogs/settingsdialog/network.h
	dialogs/settingsdialog/notifications.cpp
	dialogs/settingsdialog/notifications.h
	dialogs/settingsdialog/page.cpp
	dialogs/settingsdialog/page.h
	dialogs/settingsdialog/parentalcontrols.cpp
	dialogs/settingsdialog/parentalcontrols.h
	dialogs/settingsdialog/proportionaltableview.cpp
	dialogs/settingsdialog/proportionaltableview.h
	dialogs/settingsdialog/userinterface.cpp
	dialogs/settingsdialog/userinterface.h
	dialogs/settingsdialog/servers.cpp
	dialogs/settingsdialog/servers.h
	dialogs/settingsdialog/shortcuts.cpp
	dialogs/settingsdialog/shortcuts.h
	dialogs/settingsdialog/tools.cpp
	dialogs/settingsdialog/tools.h
	dialogs/startdialog.cpp
	dialogs/startdialog.h
	dialogs/startdialog/browse.cpp
	dialogs/startdialog/browse.h
	dialogs/startdialog/create.cpp
	dialogs/startdialog/create.h
	dialogs/startdialog/host.cpp
	dialogs/startdialog/host.h
	dialogs/startdialog/join.cpp
	dialogs/startdialog/join.h
	dialogs/startdialog/links.cpp
	dialogs/startdialog/links.h
	dialogs/startdialog/page.cpp
	dialogs/startdialog/page.h
	dialogs/startdialog/recent.cpp
	dialogs/startdialog/recent.h
	dialogs/startdialog/welcome.cpp
	dialogs/startdialog/welcome.h
	dialogs/systeminfodialog.cpp
	dialogs/systeminfodialog.h
	dialogs/tablettester.cpp
	dialogs/tablettester.h
	dialogs/touchtestdialog.cpp
	dialogs/touchtestdialog.h
	dialogs/userinfodialog.cpp
	dialogs/userinfodialog.h
	docks/brushpalettedelegate.cpp
	docks/brushpalettedelegate.h
	docks/brushpalettedock.cpp
	docks/brushpalettedock.h
	docks/colorpalette.cpp
	docks/colorpalette.h
	docks/colorsliders.cpp
	docks/colorsliders.h
	docks/colorspinner.cpp
	docks/colorspinner.h
	docks/dockbase.cpp
	docks/dockbase.h
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
	icons/drawpile.png
	icons/drawpile.svg
	scene/annotationitem.cpp
	scene/annotationitem.h
	scene/arrows_data.h
	scene/baseitem.cpp
	scene/baseitem.h
	scene/canvasitem.cpp
	scene/canvasitem.h
	scene/canvasscene.cpp
	scene/canvasscene.h
	scene/canvasview.cpp
	scene/canvasview.h
	scene/catchupitem.cpp
	scene/catchupitem.h
	scene/lasertrailitem.cpp
	scene/lasertrailitem.h
	scene/noticeitem.cpp
	scene/noticeitem.h
	scene/outlineitem.cpp
	scene/outlineitem.h
	scene/scenewrapper.cpp
	scene/scenewrapper.h
	scene/selectionitem.cpp
	scene/selectionitem.h
	scene/toggleitem.cpp
	scene/toggleitem.h
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
	toolwidgets/pansettings.cpp
	toolwidgets/pansettings.h
	toolwidgets/selectionsettings.cpp
	toolwidgets/selectionsettings.h
	toolwidgets/toolsettings.cpp
	toolwidgets/toolsettings.h
	toolwidgets/zoomsettings.cpp
	toolwidgets/zoomsettings.h
	ui/abusereport.ui
	ui/addserverdialog.ui
	ui/avatarimport.ui
	ui/brushdock.ui
	ui/brushpresetproperties.ui
	ui/canvasshortcutsdialog.ui
	ui/certificateview.ui
	ui/dumpplayback.ui
	ui/filetypedialog.ui
	ui/fillsettings.ui
	ui/flipbook.ui
	ui/inspectorsettings.ui
	ui/invitedialog.ui
	ui/lasersettings.ui
	ui/layerproperties.ui
	ui/layoutsdialog.ui
	ui/logindialog.ui
	ui/netstats.ui
	ui/playback.ui
	ui/resetsession.ui
	ui/resizedialog.ui
	ui/selectsettings.ui
	ui/serverlog.ui
	ui/sessionsettings.ui
	ui/tablettest.ui
	ui/textsettings.ui
	ui/userinfodialog.ui
	utils/animationrenderer.cpp
	utils/animationrenderer.h
	utils/accountlistmodel.cpp
	utils/accountlistmodel.h
	utils/actionbuilder.h
	utils/globalkeyeventfilter.cpp
	utils/globalkeyeventfilter.h
	utils/listserverdelegate.cpp
	utils/listserverdelegate.h
	utils/mandatoryfields.cpp
	utils/mandatoryfields.h
	utils/qtguicompat.h
	utils/recents.cpp
	utils/recents.h
	utils/widgetutils.cpp
	utils/widgetutils.h
	view/canvascontroller.cpp
	view/canvascontroller.h
	view/canvasscene.cpp
	view/canvasscene.h
	view/canvasview.cpp
	view/canvasview.h
	view/canvaswrapper.cpp
	view/canvaswrapper.h
	view/cursor.h
	view/glcanvas.cpp
	view/glcanvas.h
	view/lock.cpp
	view/lock.h
	view/softwarecanvas.cpp
	view/softwarecanvas.h
	view/viewwrapper.cpp
	view/viewwrapper.h
	widgets/brushpreview.cpp
	widgets/brushpreview.h
	widgets/canvasframe.cpp
	widgets/canvasframe.h
	widgets/canvasshortcutedit.cpp
	widgets/canvasshortcutedit.h
	widgets/colorbutton.cpp
	widgets/colorbutton.h
	widgets/curvewidget.cpp
	widgets/curvewidget.h
	widgets/dualcolorbutton.cpp
	widgets/dualcolorbutton.h
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
	widgets/mypaintinput.cpp
	widgets/mypaintinput.h
	widgets/netstatus.cpp
	widgets/netstatus.h
	widgets/notifbar.cpp
	widgets/notifbar.h
	widgets/palettewidget.cpp
	widgets/palettewidget.h
	widgets/popupmessage.cpp
	widgets/popupmessage.h
	widgets/presetselector.cpp
	widgets/presetselector.h
	widgets/recentscroll.cpp
	widgets/recentscroll.h
	widgets/resizerwidget.cpp
	widgets/resizerwidget.h
	widgets/rulerwidget.cpp
	widgets/rulerwidget.h
	widgets/serverlogview.cpp
	widgets/serverlogview.h
	widgets/spanawaretreeview.cpp
	widgets/spanawaretreeview.h
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
	widgets/viewstatusbar.cpp
	widgets/viewstatusbar.h
	widgets/zoomslider.cpp
	widgets/zoomslider.h
)

target_link_libraries(drawpile PRIVATE
	cmake-config
	dpshared
	dpclient
	drawdance
	${QT_PACKAGE_NAME}::Core
	${QT_PACKAGE_NAME}::Gui
	${QT_PACKAGE_NAME}::OpenGL
	# Svg and Sql are needed only indirectly and are linked transitively, but
	# Android and Emscripten need them here to load their plugins properly.
	${QT_PACKAGE_NAME}::Sql
	${QT_PACKAGE_NAME}::Svg
	${QT_PACKAGE_NAME}::Widgets
	${QT_PACKAGE_NAME}::Multimedia
)

if(QT_VERSION VERSION_GREATER_EQUAL 6)
	target_link_libraries(drawpile PRIVATE ${QT_PACKAGE_NAME}::OpenGLWidgets)
endif()

if(WIN32)
	target_sources(drawpile PRIVATE
		icons/drawpile.ico
		utils/wineventfilter.h
		utils/wineventfilter.cpp
	)
	add_library(drawpile-kis_tablet OBJECT)
	target_sources(drawpile-kis_tablet PRIVATE
		bundled/kis_tablet/debug.h
		bundled/kis_tablet/kis_tablet_support_win_p.h
		bundled/kis_tablet/kis_tablet_support_win.cpp
		bundled/kis_tablet/kis_tablet_support_win.h
		bundled/kis_tablet/kis_tablet_support_win8.cpp
		bundled/kis_tablet/kis_tablet_support_win8.h
		bundled/kis_tablet/pktdef.h
		bundled/kis_tablet/wintab.h
	)
	target_link_libraries(drawpile-kis_tablet PUBLIC
		${QT_PACKAGE_NAME}::Core
		${QT_PACKAGE_NAME}::Gui
		${QT_PACKAGE_NAME}::Network
		${QT_PACKAGE_NAME}::Widgets
	)
	set_target_properties(drawpile-kis_tablet
		PROPERTIES DP_AUTO_SOURCE_TREE_BASE bundled/kis_tablet
	)
	target_disable_all_warnings(drawpile-kis_tablet)
	target_link_libraries(drawpile PRIVATE drawpile-kis_tablet)
	set_property(
		INSTALL "$<TARGET_FILE_NAME:drawpile>"
		PROPERTY CPACK_DESKTOP_SHORTCUTS Drawpile
	)
# https://gitlab.kitware.com/cmake/cmake/-/issues/19813
elseif(APPLE)
	target_sources(drawpile PRIVATE
		icons/drawpile.iconset/icon_512x512@2x.png
		utils/macui.h
		utils/macui.mm
		widgets/macmenu.cpp
		widgets/macmenu.h
	)
	target_link_libraries(drawpile PRIVATE
		"-framework Cocoa"
	)
endif()

# On Qt6, private headers are required to switch between Windows Ink and Wintab.
if(WIN32 AND QT_VERSION_MAJOR VERSION_EQUAL 6)
	# TODO: Check this is doing the right thing
	target_include_directories(drawpile PRIVATE "${Qt6Gui_PRIVATE_INCLUDE_DIRS}")
endif()

# Some special handling for Android:
# * It can't deal with multiple main windows properly, trying to open a second
#	one hides the first with no way to switch between them.
# * The multi-line chat edit with history keyboard shortcuts doesn't work well
#   on Android, so we use a simpler, single-line edit instead.
# * It can't export multiple frames or spawn an external ffmpeg process, so the
#   video exporter isn't compiled in.
# * Because of restrictions in file handling, we need an extra dialog to ask the
#   user for a file name and an extension before we open the native file dialog.
# * Including XMLs for launcher icons, S-Pen support and other such frills.
# * Linking Qt's Android Extras library, which is used to grab wake and network
#   locks in Qt5. In Qt6, the JNI stuff is in Core instead.
if(ANDROID)
	target_compile_definitions(drawpile PRIVATE
		HAVE_CHAT_LINE_EDIT_MOBILE=1
		SINGLE_MAIN_WINDOW=1
	)
	target_sources(drawpile PRIVATE
		chat/chatlineeditmobile.cpp
		chat/chatlineeditmobile.h
		icons/android/drawable-anydpi-v26/ic_launcher.xml
		icons/android/drawable-anydpi/ic_launcher.xml
		icons/android/drawable-anydpi/ic_launcher_foreground.xml
		icons/android/drawable-anydpi/ic_launcher_monochrome.xml
		icons/android/values/strings.xml
		icons/android/xml/remote_action.xml
	)
	if(TARGET ${QT_PACKAGE_NAME}::AndroidExtras)
		target_link_libraries(drawpile PRIVATE ${QT_PACKAGE_NAME}::AndroidExtras)
	endif()
	target_link_libraries(drawpile PRIVATE GLESv2)
elseif(EMSCRIPTEN)
	target_compile_definitions(drawpile PRIVATE
		HAVE_CHAT_LINE_EDIT_MOBILE=1
		HAVE_EMULATED_BITMAP_CURSOR=1
		SINGLE_MAIN_WINDOW=1
	)
	target_sources(drawpile PRIVATE
		chat/chatlineeditmobile.cpp
		chat/chatlineeditmobile.h
		scene/cursoritem.cpp
		scene/cursoritem.h
	)
else()
	target_compile_definitions(drawpile PRIVATE HAVE_VIDEO_EXPORT=1)
	target_sources(drawpile PRIVATE
		chat/chatlineedit.cpp
		chat/chatlineedit.h
		dialogs/videoexportdialog.cpp
		dialogs/videoexportdialog.h
		ui/videoexport.ui
	)
	# On Windows and Linux, we want to start a new process when opening a new
	# window, since that's more resilient against crashes. Pretty sure this
	# would be unexpected on macOS, where multiple windows in a single process
	# share a common menu, so we don't do it there.
	if(NOT APPLE)
		target_compile_definitions(drawpile PRIVATE HAVE_RUN_IN_NEW_PROCESS=1)
	endif()
endif()

if(${QT_PACKAGE_NAME}Keychain_FOUND)
	target_compile_definitions(drawpile PRIVATE HAVE_QTKEYCHAIN)
	if(TARGET ${QT_PACKAGE_NAME}Keychain::${QT_PACKAGE_NAME}Keychain)
		target_link_libraries(drawpile PRIVATE ${QT_PACKAGE_NAME}Keychain::${QT_PACKAGE_NAME}Keychain)
	else()
		# QtKeychain < 0.13, included in Debian 11 and earlier, has no TARGET
		target_link_libraries(drawpile PRIVATE ${QTKEYCHAIN_LIBRARIES})
	endif()
endif()
add_feature_info("Client password manager" ${QT_PACKAGE_NAME}Keychain_FOUND "")

if(TARGET QtColorWidgets::QtColorWidgets)
	target_link_libraries(drawpile PRIVATE QtColorWidgets::QtColorWidgets)
else()
	add_library(drawpile-QtColorWidgets OBJECT)
	target_sources(drawpile-QtColorWidgets PRIVATE
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
	target_include_directories(drawpile-QtColorWidgets SYSTEM PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/bundled/QtColorWidgets/include"
	)
	target_compile_definitions(drawpile-QtColorWidgets PUBLIC
		QTCOLORWIDGETS_STATICALLY_LINKED
	)
	target_link_libraries(drawpile-QtColorWidgets PUBLIC
		${QT_PACKAGE_NAME}::Core
		${QT_PACKAGE_NAME}::Gui
		${QT_PACKAGE_NAME}::Widgets
	)
	target_disable_all_warnings(drawpile-QtColorWidgets)
	set_target_properties(drawpile-QtColorWidgets
		PROPERTIES DP_AUTO_SOURCE_TREE_BASE bundled/QtColorWidgets
	)

	target_link_libraries(drawpile PRIVATE drawpile-QtColorWidgets)
endif()
add_feature_info("External QtColorWidgets" "TARGET QtColorWidgets::QtColorWidgets" "")

if(NOT EMSCRIPTEN)
	target_sources(drawpile PRIVATE
		dialogs/startdialog/updatenotice.cpp
		dialogs/startdialog/updatenotice.h
	)
	dp_install_executables(TARGETS drawpile INCLUDE_BUNDLEDIR)
endif()

if(WIN32)
	configure_file(drawpile.rc.in drawpile.rc)
	configure_file(drawpile.exe.manifest.in drawpile.exe.manifest)
	target_sources(drawpile PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}/drawpile.exe.manifest"
		"${CMAKE_CURRENT_BINARY_DIR}/drawpile.rc"
	)

	# On Windows the assets must be set up next to the output binary at build
	# time for developers to be able to run the application without missing
	# assets and without running `cmake --install`
	set(build_time_assets_dir "$<TARGET_FILE_DIR:drawpile>/data")
elseif(APPLE)
	set_target_properties(drawpile PROPERTIES
		RUNTIME_OUTPUT_NAME Drawpile
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
		MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
		MACOSX_BUNDLE_COPYRIGHT "Copyright © Drawpile contributors"
		MACOSX_BUNDLE_GUI_IDENTIFIER ${gui_identifier}
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
elseif(ANDROID)
	set_target_properties(drawpile PROPERTIES
		# Qt5 requires the output name to match the project name; Qt6 does not
		# but might as well be consistent
		OUTPUT_NAME ${PROJECT_NAME}
		# This normally only applies to Qt6 but AddResources also uses it to
		# find the place where assets go
		QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/android"
	)

	# On Android, the assets must be set up in advance in one directory because
	# Qt only accepts one directory for all extra files and puts them in the APK
	# at build time
	set(build_time_assets_dir "${CMAKE_CURRENT_BINARY_DIR}/android/assets")
	# This directory will get translations and other generated files added to it
	# so it cannot be just a symlink to the source assets directory
	file(MAKE_DIRECTORY "${build_time_assets_dir}")
	set(output_apk "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_PROPERTY:drawpile,OUTPUT_NAME>-${PROJECT_VERSION}-${ANDROID_ABI}.apk")
	include(GetSharedLibs)
	get_shared_libs(android_extra_libs drawpile)

	if(QT_VERSION VERSION_GREATER_EQUAL 6)
		set_property(TARGET drawpile APPEND PROPERTY
			QT_ANDROID_EXTRA_LIBS ${android_extra_libs}
		)
		set(ANDROID_QT_NAMESPACE org.qtproject.qt)
		file(READ AndroidManifest.xml.qt6 ANDROID_EXTRA_METADATA)
		qt_android_generate_deployment_settings(drawpile)
		qt_android_add_apk_target(drawpile)
		add_custom_command(TARGET drawpile_make_apk POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"android-build/drawpile.apk"
				"${output_apk}"
			COMMAND_EXPAND_LISTS
			COMMENT "Copying APK to runtime output directory"
			VERBATIM
		)
	else()
		set(ANDROID_QT_NAMESPACE org.qtproject.qt5)
		file(READ AndroidManifest.xml.qt5 ANDROID_EXTRA_METADATA)
		target_link_libraries(drawpile PRIVATE ${QT_PACKAGE_NAME}::AndroidExtras)
		add_custom_target(drawpile_copy_apk ALL
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${CMAKE_BINARY_DIR}/android-build/${PROJECT_NAME}.apk"
				"${output_apk}"
			COMMAND_EXPAND_LISTS
			COMMENT "Copying APK to runtime output directory"
			VERBATIM
		)
		add_dependencies(drawpile_copy_apk apk)

		# We need to perform some disgusting hacks on the deployment settings
		# file because of Qt5's utter inability to generate it correctly. Qt6
		# does this properly, so hopefully this will be going away eventually.
		file(STRINGS "${CMAKE_BINARY_DIR}/android_deployment_settings.json" cursed_json)

		# It fails to include the build tools revision altogether. In qmake it
		# would do it correctly, but in CMake it just leaves it out. This leads
		# to androiddeployqt unable to find zipalign and apksigner. The logic
		# used here to figure out the version mirrors what it would do in qmake.
		if(NOT ANDROID_SDK_BUILD_TOOLS_REVISION)
			file(
				GLOB build_tools_dirs
				RELATIVE "${ANDROID_SDK}/build-tools"
				"${ANDROID_SDK}/build-tools/*"
			)
			foreach(build_tools_dir IN LISTS build_tools_dirs)
				if(NOT ANDROID_SDK_BUILD_TOOLS_REVISION OR
						build_tools_dir VERSION_GREATER ANDROID_SDK_BUILD_TOOLS_REVISION)
					set(ANDROID_SDK_BUILD_TOOLS_REVISION "${build_tools_dir}")
				endif()
			endforeach()
		endif()
		list(INSERT cursed_json 1
			"\"sdkBuildToolsRevision\": \"${ANDROID_SDK_BUILD_TOOLS_REVISION}\",")

		# We cannot look to see if Qt5Keychain exists or not without causing
		# `Qt5AndroidSupport.cmake` to write the JSON file that needs the
		# variable we are trying to generate.
		if(android_extra_libs)
			list(JOIN android_extra_libs "," android_extra_libs)
			list(INSERT cursed_json 1 "\"android-extra-libs\": \"${android_extra_libs}\",")
		endif()

		file(WRITE "${CMAKE_BINARY_DIR}/android_deployment_settings.json" ${cursed_json})
	endif()

	include(DrawpileFileExtensions)
	set(ANDROID_APPLICATION_ATTRIBUTES "android:icon=\"@drawable/ic_launcher\"")
	set(ANDROID_PACKAGE_NAMESPACE net.drawpile)
	get_android_extensions(ANDROID_VIEW_INTENTS)

	configure_file(AndroidManifest.xml.in
		"${CMAKE_CURRENT_BINARY_DIR}/android/AndroidManifest.xml"
	)

	file(CREATE_LINK
		"${CMAKE_CURRENT_SOURCE_DIR}/icons/android"
		"${CMAKE_CURRENT_BINARY_DIR}/android/res"
		SYMBOLIC
	)

	# This directory will get translations and other generated files added to it
	# so it cannot be just a symlink to the source assets directory
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/android/assets")

	install(FILES ${output_apk} DESTINATION .)
elseif(EMSCRIPTEN)
	set_target_properties(drawpile PROPERTIES
		# We use PATH in init.js to extract the directory name for assets.
		QT_WASM_EXTRA_EXPORTED_METHODS "PATH"
		# 128MB, to avoid regrowing memory on startup. Default would be 50MB.
		QT_WASM_INITIAL_MEMORY "134217728")

	set(em_files_to_install
		"icons/drawpile.svg"
		"icons/favicon.ico"
		"wasm/drawpile.css"
	)
	foreach(em_file IN LISTS em_files_to_install)
		set(em_file_source "${CMAKE_CURRENT_SOURCE_DIR}/${em_file}")
		get_filename_component(em_file_base "${em_file}" NAME)
		configure_file(
			"${CMAKE_CURRENT_SOURCE_DIR}/${em_file}"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${em_file_base}"
			COPYONLY
		)
	endforeach()

	find_program(NPM_COMMAND npm REQUIRED)
	add_custom_command(
		OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/init.js"
		COMMAND
			"${NPM_COMMAND}" exec -- esbuild --bundle --minify
			"--outfile=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/init.js"
			init.js
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/wasm"
		DEPENDS
			drawpile
			"${CMAKE_CURRENT_SOURCE_DIR}/wasm/init.js"
		COMMAND_EXPAND_LISTS
		COMMENT "Bundling init.js"
		VERBATIM
	)
	add_custom_target(
		drawpile_init_js
		DEPENDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/init.js"
	)

	set(build_time_assets_dir "${CMAKE_CURRENT_BINARY_DIR}/wasm/assets")
	add_custom_command(
		OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets.bundle"
		COMMAND
			"${CMAKE_CURRENT_SOURCE_DIR}/wasm/generate-assets-bundle.py"
			"${CMAKE_CURRENT_BINARY_DIR}/wasm/assets"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets.bundle"
		DEPENDS
			drawpile
		COMMAND_EXPAND_LISTS
		COMMENT "Generating assets.bundle"
		VERBATIM
	)
	add_custom_target(
		drawpile_assets_bundle
		DEPENDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets.bundle"
	)

	add_custom_command(
		OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html"
		COMMAND
			${CMAKE_COMMAND}
			"-DINPUT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/wasm/index.html"
			"-DOUTPUT_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html"
			"-DASSETS_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets.bundle"
			"-DWASM_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drawpile.wasm"
			-P "${PROJECT_SOURCE_DIR}/cmake/GenerateIndexHtml.cmake"
		DEPENDS
			drawpile_assets_bundle
			drawpile_init_js
			"${CMAKE_CURRENT_SOURCE_DIR}/icons/drawpile.svg"
			"${CMAKE_CURRENT_SOURCE_DIR}/wasm/drawpile.css"
			"${CMAKE_CURRENT_SOURCE_DIR}/wasm/index.html"
			"${CMAKE_CURRENT_SOURCE_DIR}/wasm/init.js"
		COMMENT "Generating index.html"
		VERBATIM
	)
	add_custom_target(
		drawpile_index_html
		ALL DEPENDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html"
	)

	install(
		FILES
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets.bundle"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drawpile.css"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drawpile.js"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drawpile.svg"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drawpile.wasm"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drawpile.worker.js"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/favicon.ico"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/init.js"
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qtloader.js"
		TYPE BIN
	)

	target_link_options(drawpile PUBLIC
		-sASYNCIFY
		-sMAXIMUM_MEMORY=4294967296
		-sEXPORTED_FUNCTIONS=_main,_drawpileMain,_drawpileInitScheduleSyncPersistentFileSystem,_drawpileFinishPersistentFileSystemSync,_drawpileHandleBrowserAuth,_drawpileHandleMouseLeave,_drawpileShouldPreventUnload,stringToNewUTF8,UTF8ToString
		--js-library "${CMAKE_CURRENT_SOURCE_DIR}/wasm/lib.js"
		-lidbfs.js
	)
	if(CMAKE_BUILD_TYPE MATCHES "^[rR][eE][lL]")
		target_link_options(drawpile PUBLIC -O3)
	endif()
elseif(UNIX)
	function(install_datarootdir target files destination)
		install(
			FILES ${files}
			DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${destination}"
			${ARGN}
		)
	endfunction()

	include(DrawpileFileExtensions)
	get_xdg_extensions(MIME_TYPES)
	configure_file(drawpile.desktop.in drawpile.desktop)
	install_datarootdir(drawpile "${CMAKE_CURRENT_BINARY_DIR}/drawpile.desktop" applications
		RENAME net.drawpile.drawpile.desktop
	)
	install_datarootdir(drawpile icons/drawpile.png icons/hicolor/256x256/apps)
	install_datarootdir(drawpile icons/drawpile.svg icons/hicolor/scalable/apps)
	install_datarootdir(drawpile drawpile.appdata.xml metainfo
		RENAME net.drawpile.drawpile.appdata.xml
	)

	generate_xdg_mime_info("${CMAKE_CURRENT_BINARY_DIR}/mime")
	install(
		DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mime"
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}"
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

include(AddResources)
add_resources(drawpile
	FILES
		hotdogstand.colors
		kritabright.colors
		kritadark.colors
		kritadarker.colors
		nightmode.colors
		initialbrushpresets.db
	DIRS palettes sounds theme
	BUILD_TIME_DESTINATION "${build_time_assets_dir}"
)

if(${QT_PACKAGE_NAME}LinguistTools_FOUND)
	add_subdirectory(i18n)

	include(DrawpileTranslations)
	bundle_translations(bundle_files
		NAME all
		OUTPUT_LOCATION i18n-bundle
		QT qtbase qtmultimedia
		TARGETS dpclient drawpile
	)

	target_sources(drawpile PRIVATE ${bundle_files})

	if(APPLE)
		set_source_files_properties(${bundle_files}
			PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/i18n"
		)
	else()
		if(build_time_assets_dir)
			add_custom_command(
				TARGET drawpile PRE_LINK
				COMMAND "${CMAKE_COMMAND}" -E make_directory
					"${build_time_assets_dir}"
				COMMAND "${CMAKE_COMMAND}" -E create_symlink
					"${CMAKE_CURRENT_BINARY_DIR}/i18n-bundle"
					"${build_time_assets_dir}/i18n"
				COMMAND_EXPAND_LISTS
				COMMENT "Creating symlink to i18n bundles in build directory"
				VERBATIM
			)
		endif()

		# Assets will be installed by Gradle on Android and are bundled on WASM.
		if(NOT ANDROID AND NOT EMSCRIPTEN)
			install(
				FILES ${bundle_files}
				DESTINATION "${INSTALL_APPDATADIR}/i18n"
			)
		endif()
	endif()
endif()

directory_auto_source_groups()
