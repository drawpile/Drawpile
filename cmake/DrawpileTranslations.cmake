#[[ This module contains data and functions for internationalisation. #]]

# Commented-out languages exist, but have no or only a negligible amount of
# translated strings so that we shouldn't ship them yet.
set(SUPPORTED_LANGS
    ar_EG # Standard Arabic (Egypt used as a stand-in)
    cs_CZ # Czech
    de_DE # German
    # el_GR # Greek
    en_US # English
    es_CO # American Spanish (Colombia used as a stand-in)
    # fa_IR # Farsi (Iran)
    fi_FI # Finnish
    # fil_PH # Filipono
    fr_FR # French
    # hi_IN # Hindi (India)
    id_ID # Indonesian
    it_IT # Italian
    ja_JP # Japanese
    ko_KR # Korean
    # mn_MN # Mongolian
    # ms_MY # Malay
    # nb_NO # Norwegian Bokmal
    pl_PL # Polish
    pt_BR # Brazilian Portuguese
    pt_PT # Portuguese Portuguese
    ru_RU # Russian
    th_TH # Thai
    tr_TR # Turkish
    uk_UA # Ukrainian
    vi_VN # Vietnamese
    zh_CN # Simplified Chinese
)

# Qt doesn't have some translations, but Drawpile does.
set(LANGS_UNSUPPORTED_IN_QT id nb vi)

define_property(TARGET PROPERTY DP_TRANSLATION_QM_FILES
	BRIEF_DOCS ".qm files for this target"
	FULL_DOCS ".qm files for this target"
)

#[[
Returns the list of supported languages in a variable that can be interpolated
into a C++ initializer list.
#]]
function(get_supported_langs_cxx out_var)
	list(JOIN SUPPORTED_LANGS "\", \"" langs)
	set(${out_var} "\"${langs}\"" PARENT_SCOPE)
endfunction()

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
		set(file ${prefix}_${lang}.ts)
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
			OPTIONS -no-obsolete -locations relative -extensions
			# Default extensions without .js, those are not Qt-related files.
			java,jui,ui,c,c++,cc,cpp,cxx,ch,h,h++,hh,hpp,hxx,qs,qml,qrc
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

	# This allows `bundle_translations` to recover the .qm files for a target,
	# which is needed because these filenames may not be the same as the target
	# name
	set_target_properties(${target} PROPERTIES
		DP_TRANSLATION_QM_FILES "${qm_files}"
	)

	# Cargo culting to hopefully not have generated i18n sources get erased
	# on `make clean`
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CLEAN_NO_CUSTOM TRUE)
endfunction()

#[[
Combines a set of translation files into bundles.
#]]
function(bundle_translations out_files)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "NAME;OUTPUT_LOCATION" "QT;TARGETS")

	find_program(
		lconvert
		NAMES "lconvert-qt${QT_VERSION_MAJOR}" "lconvert"
		PATHS "/usr/lib/qt${QT_VERSION_MAJOR}/bin"
		REQUIRED)

	if(NOT ARG_NAME)
		message(FATAL_ERROR "Missing required NAME for translation bundle")
	endif()

	if(NOT ARG_OUTPUT_LOCATION)
		message(FATAL_ERROR "Missing required OUTPUT_LOCATION for translation bundle")
	endif()

	if(NOT IS_ABSOLUTE "${ARG_OUTPUT_LOCATION}")
		set(ARG_OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${ARG_OUTPUT_LOCATION}")
	endif()

	file(MAKE_DIRECTORY "${ARG_OUTPUT_LOCATION}")

	set(outputs "")
	foreach(lang IN LISTS SUPPORTED_LANGS ITEMS en_US)
		string(SUBSTRING ${lang} 0 2 lang_code)
		set(files "")
		foreach(module IN LISTS ARG_QT)
			_get_qt_translation_file(file ${module} ${lang})
			if(file)
				list(APPEND files "${file}")
			endif()
		endforeach()
		foreach(target IN LISTS ARG_TARGETS)
			get_target_property(binary_dir ${target} BINARY_DIR)
			get_target_property(qm_files ${target} DP_TRANSLATION_QM_FILES)
			foreach(qm_file IN LISTS qm_files)
				if(qm_file MATCHES "_(${lang}|${lang_code})\\.qm$")
					list(APPEND files "${qm_file}")
				endif()
			endforeach()
		endforeach()

		if(files)
			set(output "${ARG_OUTPUT_LOCATION}/${ARG_NAME}_${lang}.qm")
			add_custom_command(
				OUTPUT "${output}"
				COMMAND "${lconvert}" -no-untranslated -o "${output}" ${files}
				DEPENDS ${files}
			)

			list(APPEND outputs "${output}")
		endif()
	endforeach()

	set(${out_files} ${outputs} PARENT_SCOPE)
endfunction()

function(_get_qt_translation_file out_file module lang)
	_get_qt_translation_dir(qm_location)
	if(qm_location)
		set(_qm_file "${qm_location}/${module}_${lang}.qm")
		if(NOT EXISTS "${_qm_file}")
			string(SUBSTRING ${lang} 0 2 lang_code)
			set(_qm_file "${qm_location}/${module}_${lang_code}.qm")
		endif()
		if(NOT EXISTS "${_qm_file}")
			# Don't warn about langauges that are known to not exist in Qt.
			if(NOT lang_code IN_LIST LANGS_UNSUPPORTED_IN_QT)
				message(WARNING "Could not find Qt translation ${_qm_file}")
			endif()
			set(_qm_file "")
		endif()
	else()
		set(_qm_file "")
	endif()
	set(${out_file} "${_qm_file}" PARENT_SCOPE)
endfunction()

function(_get_qt_translation_dir out_path)
	# We only want to attempt to find the translation directory once per run and
	# then print one warning if that's not found, so cache it in a property.
	get_property(have_qm_location GLOBAL PROPERTY dp_qm_location SET)
	if(have_qm_location)
		get_property(qm_location GLOBAL PROPERTY dp_qm_location)
	else()
		if(QT_VERSION_MAJOR VERSION_EQUAL 6)
			# Qt6 defines its install locations in variables.
			get_filename_component(qm_location
				"${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}" ABSOLUTE)
		else()
			# Qt5 requires us to run qmake to query the translations directory.
			get_target_property(qmake_exe Qt5::qmake IMPORTED_LOCATION)
			execute_process(
				COMMAND "${qmake_exe}" -query QT_INSTALL_TRANSLATIONS
				OUTPUT_VARIABLE qm_location OUTPUT_STRIP_TRAILING_WHITESPACE)
		endif()

		if(NOT EXISTS "${qm_location}")
			message(WARNING "Qt translations directory '${qm_location}' not found")
			set(qm_location "")
		endif()
		set_property(GLOBAL PROPERTY dp_qm_location "${qm_location}")
	endif()
	set(${out_path} "${qm_location}" PARENT_SCOPE)
endfunction()
