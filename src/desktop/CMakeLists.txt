find_package(${QT_PACKAGE_NAME} REQUIRED COMPONENTS Gui Svg Widgets Multimedia)
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
else()
	add_executable(drawpile MACOSX_BUNDLE)
endif()

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
	dialogs/curvepresetdialog.cpp
	dialogs/curvepresetdialog.h
	dialogs/dumpplaybackdialog.cpp
	dialogs/dumpplaybackdialog.h
	dialogs/flipbook.cpp
	dialogs/flipbook.h
	dialogs/hostdialog.cpp
	dialogs/hostdialog.h
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
	dialogs/sessionundodepthlimitdialog.cpp
	dialogs/sessionundodepthlimitdialog.h
	dialogs/sessionsettings.cpp
	dialogs/sessionsettings.h
	dialogs/settingsdialog.cpp
	dialogs/settingsdialog.h
	dialogs/settingsdialog/contentfilter.cpp
	dialogs/settingsdialog/contentfilter.h
	dialogs/settingsdialog/general.cpp
	dialogs/settingsdialog/general.h
	dialogs/settingsdialog/helpers.h
	dialogs/settingsdialog/input.cpp
	dialogs/settingsdialog/input.h
	dialogs/settingsdialog/network.cpp
	dialogs/settingsdialog/network.h
	dialogs/settingsdialog/servers.cpp
	dialogs/settingsdialog/servers.h
	dialogs/settingsdialog/shortcuts.cpp
	dialogs/settingsdialog/shortcuts.h
	dialogs/settingsdialog/tools.cpp
	dialogs/settingsdialog/tools.h
	dialogs/tablettester.cpp
	dialogs/tablettester.h
	dialogs/userinfodialog.cpp
	dialogs/userinfodialog.h
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
	ui/tablettest.ui
	ui/textsettings.ui
	ui/userinfodialog.ui
	utils/actionbuilder.h
	utils/hidedocktitlebarseventfilter.cpp
	utils/hidedocktitlebarseventfilter.h
	utils/listserverdelegate.cpp
	utils/listserverdelegate.h
	utils/mandatoryfields.cpp
	utils/mandatoryfields.h
	utils/qtguicompat.h
	utils/recentfiles.cpp
	utils/recentfiles.h
	utils/sanerformlayout.cpp
	utils/sanerformlayout.h
	utils/widgetutils.h
	widgets/brushpreview.cpp
	widgets/brushpreview.h
	widgets/canvasshortcutedit.cpp
	widgets/canvasshortcutedit.h
	widgets/colorbutton.cpp
	widgets/colorbutton.h
	widgets/curvewidget.cpp
	widgets/curvewidget.h
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
)

target_link_libraries(drawpile PRIVATE
	cmake-config
	dpshared
	dpclient
	Drawdance::dpengine
	Drawdance::dpmsg
	${QT_PACKAGE_NAME}::Core
	${QT_PACKAGE_NAME}::Gui
	# Svg is used only indirectly as an image loader plugin, but e.g.
	# androiddeployqt needs to see it linked in order to include the libraries
	# in the output
	${QT_PACKAGE_NAME}::Svg
	${QT_PACKAGE_NAME}::Widgets
	${QT_PACKAGE_NAME}::Multimedia
)

if(VERSION_CHECK)
	target_sources(drawpile PRIVATE
		dialogs/versioncheckdialog.cpp
		dialogs/versioncheckdialog.h
		ui/versioncheck.ui
	)
	target_compile_definitions(drawpile PRIVATE ENABLE_VERSION_CHECK)
endif()

if(WIN32)
	target_sources(drawpile PRIVATE icons/drawpile.ico)
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

if(ANDROID)
	# Android doesn't sensibly support multiple windows, they just cover each
	# other up with no way to switch between them, so we don't allow that there.
	target_compile_definitions(drawpile PRIVATE SINGLE_MAIN_WINDOW=1)
	target_sources(drawpile PRIVATE
		dialogs/androidfiledialog.cpp
		dialogs/androidfiledialog.h
		icons/android/drawable-anydpi-v26/ic_launcher.xml
		icons/android/drawable-anydpi/ic_launcher.xml
		icons/android/drawable-anydpi/ic_launcher_foreground.xml
		icons/android/drawable-anydpi/ic_launcher_monochrome.xml
		icons/android/values/strings.xml
		icons/android/xml/remote_action.xml
		ui/androidfiledialog.ui
	)
	if(TARGET ${QT_PACKAGE_NAME}::AndroidExtras)
		target_link_libraries(drawpile PRIVATE ${QT_PACKAGE_NAME}::AndroidExtras)
	endif()
else()
	# Android can neither deal with exporting multiple files nor can it spawn an
	# ffmpeg process for video recording, so we only allow it outside of it.
	target_sources(drawpile PRIVATE
		dialogs/videoexportdialog.cpp
		dialogs/videoexportdialog.h
		ui/videoexport.ui
	)
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
	target_compile_definitions(drawpile-QtColorWidgets INTERFACE QTCOLORPICKER_STATICALLY_LINKED)
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

# On Android the target is a non-installable library
if(NOT ANDROID)
	install(TARGETS drawpile
		BUNDLE DESTINATION "${INSTALL_BUNDLEDIR}"
	)
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
	set(output_apk "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_PROPERTY:drawpile,OUTPUT_NAME>-${PROJECT_VERSION}.apk")
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

		# This disgusting hack is because we cannot look to see if Qt5Keychain
		# exists or not without causing `Qt5AndroidSupport.cmake` to write the
		# JSON file that needs the variable we are trying to generate. Since
		# Qt6 is not this cursed, this will be going away eventually so is fine.
		if(android_extra_libs)
			list(JOIN android_extra_libs "," android_extra_libs)
			file(STRINGS "${CMAKE_BINARY_DIR}/android_deployment_settings.json" cursed_json)
			list(INSERT cursed_json 1 "\"android-extra-libs\": \"${android_extra_libs}\",")
			file(WRITE "${CMAKE_BINARY_DIR}/android_deployment_settings.json" ${cursed_json})
		endif()
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
	install_datarootdir(drawpile appdata.xml metainfo
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

		# Assets will be installed by Gradle on Android
		if(NOT ANDROID)
			install(
				FILES ${bundle_files}
				DESTINATION "${INSTALL_APPDATADIR}/i18n"
			)
		endif()
	endif()
endif()

directory_auto_source_groups()
