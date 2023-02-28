set(SUPPORTED_LANGS cs_CZ de_DE fi_FI fr_FR it_IT ja_JP pt_BR ru_RU uk_UA vi_VN zh_CN)

# Using a variable name used anywhere else as a parameter causes CMake to
# erase the parent one
function(_add_translation _qm_files)
	if(QT_VERSION VERSION_GREATER_EQUAL 5.15)
		qt_add_translation("${_qm_files}" ${ARGN})
	else()
		qt5_add_translation("${_qm_files}" ${ARGN})
	endif()
	set("${_qm_files}" "${${_qm_files}}" PARENT_SCOPE)
endfunction()

function(_create_translation _qm_files)
	if(QT_VERSION VERSION_GREATER_EQUAL 5.15)
		qt_create_translation("${_qm_files}" ${ARGN})
	else()
		qt5_create_translation("${_qm_files}" ${ARGN})
	endif()
	set("${_qm_files}" "${${_qm_files}}" PARENT_SCOPE)
endfunction()

#[[
Adds Qt translations with the given bundle prefix to a target.
#]]
function(target_add_translations target prefix)
	if(NOT target)
		message(FATAL_ERROR "missing required target")
		return()
	endif()

	set(ts_files "")

	foreach(lang IN LISTS SUPPORTED_LANGS)
		string(SUBSTRING ${lang} 0 2 lang_code)

		set(file ${prefix}_${lang_code}.ts)
		list(APPEND ts_files ${file})
		if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${file})
			message(STATUS "Creating new empty translation file ${file}")
			file(CONFIGURE @ONLY
				NEWLINE_STYLE UNIX
				OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${file}
				CONTENT [[<?xml version="1.0" encoding="utf-8"?>
					<!DOCTYPE TS>
					<TS version="2.1" language="@lang@"></TS>
				]])
		endif()
	endforeach()

	if(UPDATE_TRANSLATIONS)
		_create_translation(qm_files "${CMAKE_CURRENT_SOURCE_DIR}/.." ${ts_files}
			OPTIONS -no-obsolete -locations relative
		)
	else()
		_add_translation(qm_files ${ts_files})
	endif()

	# Using a custom target is required to get CMake generate anything. Trying
	# to add .qm files directly to the target causes "Cannot find source file"
	# even though other projects seem to do this without a problem somehow
	add_custom_target(${target}-i18n DEPENDS ${qm_files})
	add_dependencies(${target} ${target}-i18n)
	target_sources(${target} PRIVATE ${ts_files})

	install(
		FILES ${qm_files}
		DESTINATION ${INSTALL_APPDATADIR}/i18n
		COMPONENT i18n
	)

	# Cargo culting to hopefully not have generated i18n sources get erased
	# on `make clean`
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CLEAN_NO_CUSTOM TRUE)
endfunction()

#[[
Installs translations from Qt.
#]]
function(install_qt_translations destination)
	get_filename_component(qm_location "${QT_DIR}/../../../translations" ABSOLUTE)
	foreach(module IN LISTS ARGN)
		foreach(lang IN LISTS SUPPORTED_LANGS ITEMS en_US)
			set(file "${qm_location}/${module}_${lang}.qm")
			if(NOT EXISTS "${file}")
				string(SUBSTRING ${lang} 0 2 lang_code)
				set(file "${qm_location}/${module}_${lang_code}.qm")
			endif()
			if(EXISTS "${file}")
				install(
					FILES ${file}
					DESTINATION ${destination}
					COMPONENT i18n
				)
			endif()
		endforeach()
	endforeach()
endfunction()
