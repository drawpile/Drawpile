find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Widgets Multimedia)
find_package(${QT_PACKAGE_NAME}ColorWidgets QUIET)
find_package(${QT_PACKAGE_NAME}Keychain QUIET)

add_executable(drawpile MACOSX_BUNDLE
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
	ui/cursors/cursors.qrc
	ui/icons/builtin.qrc
	$<$<BOOL:${APPLE}>:widgets/macmenu.cpp>
)

target_include_directories(drawpile PRIVATE "../libclient")

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
	if(WIN32)
		target_sources(drawpile PRIVATE
			bundled/kis_tablet/kis_tablet_support_win8.cpp
			bundled/kis_tablet/kis_tablet_support_win.cpp
		)
	elseif(NOT APPLE)
		find_package(X11 REQUIRED COMPONENTS Xi xcb)
		find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS X11Extras)

		target_sources(drawpile PRIVATE
			bundled/kis_tablet/kis_xi2_event_filter.cpp
			bundled/kis_tablet/qxcbconnection.cpp
			bundled/kis_tablet/qxcbconnection_xi2.cpp
		)

		target_link_libraries(drawpile
			${QT_PACKAGE_NAME}::X11Extras
			X11::Xi
			X11::xcb
		)
	endif()
endif()

if(${QT_PACKAGE_NAME}Keychain_FOUND)
	target_compile_definitions(drawpile PRIVATE HAVE_QTKEYCHAIN)
	target_link_libraries(drawpile ${QT_PACKAGE_NAME}Keychain::${QT_PACKAGE_NAME}Keychain)
endif()
add_feature_info("Client password manager" ${QT_PACKAGE_NAME}Keychain_FOUND "")

if(QTCOLORWIDGETS_FOUND)
	target_include_directories(drawpile SYSTEM PRIVATE
		${LIBQTCOLORWIDGETS_INCLUDE_DIRS}
	)
	target_link_libraries(drawpile ${LIBQTCOLORWIDGETS_LIBRARIES})
else()
	target_sources(drawpile PRIVATE
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
		bundled/QtColorWidgets/color_widgets.qrc
	)
	target_include_directories(drawpile SYSTEM PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/bundled/QtColorWidgets/include"
	)
	target_compile_definitions(drawpile PRIVATE QTCOLORPICKER_STATICALLY_LINKED)
endif()
add_feature_info("External QtColorWidgets" QTCOLORWIDGETS_FOUND "")

if(WIN32)
	configure_file(drawpile.rc.in drawpile.rc)
	target_sources(drawpile PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/drawpile.rc)
elseif(APPLE)
	function(add_desktop_resources target)
		set(multiValueArgs FILES DIRS)
		cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "${multiValueArgs}")

		if(NOT target)
			message(FATAL_ERROR "missing required target")
			return()
		endif()

		foreach(dir IN LISTS ARG_DIRS)
			file(GLOB resources LIST_DIRECTORIES FALSE "assets/${dir}/*")
			target_sources(${target} PRIVATE ${resources})
			set_source_files_properties(${resources}
				PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${dir}"
			)
		endforeach()

		foreach(file IN LISTS ARG_FILES)
			target_sources(${target} PRIVATE
				assets/${file}
			)
			set_property(TARGET ${target} APPEND PROPERTY RESOURCE
				assets/${file}
			)
		endforeach()
	endfunction()

	set_property(TARGET drawpile PROPERTY RUNTIME_OUTPUT_NAME Drawpile)
	set(MACOSX_BUNDLE_INFO_PLIST DrawpileInfo.plist.in)
	set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
	set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_GIT_REVISION})
	set(MACOSX_BUNDLE_COPYRIGHT "Copyright © Drawpile contributors")
	set(MACOSX_BUNDLE_GUI_IDENTIFIER net.drawpile.DrawpileClient)
	set(MACOSX_BUNDLE_ICON_FILE drawpile.icns)
	set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
	set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})

	# TODO: This should be generic and then use install(TARGETS... RESOURCE)
	add_desktop_resources(drawpile
		FILES initialbrushpresets.db
		DIRS "theme/dark" "theme/light" palettes sounds theme
	)

	add_custom_command(
		OUTPUT drawpile.icns
		DEPENDS assets/drawpile.iconset/icon_512x512@2x.png
		COMMAND iconutil -c icns -o drawpile.icns "${CMAKE_CURRENT_SOURCE_DIR}/assets/drawpile.iconset"
	)
	target_sources(drawpile PRIVATE drawpile.icns)
	set_property(TARGET drawpile APPEND PROPERTY RESOURCE drawpile.icns)

	find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} QUIET COMPONENTS LinguistTools)
	if(${QT_PACKAGE_NAME}LinguistTools_FOUND)
		add_custom_command(TARGET drawpile POST_BUILD
			DEPENDS dpclient
			COMMENT "Copying OS X i18n content"
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"$<TARGET_PROPERTY:dpclient,BINARY_DIR>/i18n"
				"$<TARGET_BUNDLE_CONTENT_DIR:drawpile>/Resources/i18n"
		)
	endif()
elseif(UNIX)
	install(TARGETS drawpile)
	install(FILES assets/net.drawpile.drawpile.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
	install(FILES assets/drawpile.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps)
	install(FILES assets/drawpile.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
	install(FILES assets/drawpile.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/mimetypes RENAME application-x-drawpile-recording.png)
	install(FILES assets/drawpile.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/mimetypes RENAME application-x-drawpile-recording.svg)
	install(FILES assets/net.drawpile.drawpile.appdata.xml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
	install(FILES assets/x-drawpile.xml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
	install(DIRECTORY assets/palettes DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/drawpile/drawpile)
	install(DIRECTORY assets/sounds DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/drawpile/drawpile)
	install(DIRECTORY assets/theme DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/drawpile/drawpile)
	install(FILES
		assets/nightmode.colors
		assets/initialbrushpresets.db
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/drawpile/drawpile)
else()
	message(FATAL_ERROR "unsupported installation target")
endif()

target_auto_source_group(drawpile)
