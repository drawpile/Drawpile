set(SUPPORTED_FILE_TYPES
	TYPE
		EXPORTED
		NAME Drawpile recording
		GROUP RECORDING
		MIME application/vnd.drawpile.recording
		UTI net.drawpile.dprec
		CONFORMS_TO public.image
		EXT dprec dprecz
		MAGIC "DPRECR\\000"

	TYPE
		EXPORTED
		NAME Drawpile text recording
		GROUP RECORDING
		MIME text/vnd.drawpile.recording
		UTI net.drawpile.dptxt
		CONFORMS_TO public.image public.text
		EXT dptxt dptxtz

	TYPE
		IMPORTED
		NAME OpenRaster image
		GROUP IMAGE
		MIME image/openraster
		REFERENCE https://www.freedesktop.org/wiki/Specifications/OpenRaster/
		UTI org.krita.ora
		EXT ora

	# Drawpile supports whatever additional formats the Rust image crate is
	# configured to support in dpimpex
	TYPE
		NAME PNG image
		GROUP IMAGE
		MIME image/png
		UTI public.png
		EXT png

	TYPE
		NAME JPEG image
		GROUP IMAGE
		MIME image/jpeg
		UTI public.jpeg
		EXT jpg jpeg jpe jif jfif jfi

	TYPE
		NAME GIF image
		GROUP IMAGE
		MIME image/gif
		UTI com.compuserve.gif
		EXT gif

	TYPE
		NAME WebP image
		GROUP IMAGE
		MIME image/webp
		UTI org.webmproject.webp
		EXT webp
)

set(SUPPORTED_URL_PROTOCOLS
	PROTOCOL
		NAME Drawpile session
		UTI net.drawpile.session.url
		SCHEME drawpile
)

set(_FT_VALUE_TOKENS NAME GROUP MIME UTI EXT REFERENCE CONFORMS_TO MAGIC)
set(_FT_FLAG_TOKENS EXPORTED IMPORTED)
set(_FT_ANY_TOKENS TYPE ${_FT_VALUE_TOKENS} ${_FT_FLAG_TOKENS})
set(_URL_VALUE_TOKENS NAME UTI SCHEME)
set(_URL_ANY_TOKENS PROTOCOL ${_URL_VALUE_TOKENS})

#[[
Gets registered file extensions and URL protocols in a format suitable for
use in Info.plist.
#]]
function(get_plist_extensions icon out_var)
	_plist_generate_extensions(exts "${icon}")
	_plist_generate_urls(urls "${icon}")
	set(${out_var} "${exts}${urls}" PARENT_SCOPE)
endfunction()

#[[
Gets extensions assigned to the given file group in a format suitable for use
by a Qt file filter.
#]]
function(get_qt_extensions group out_var)
	set(exts "")
	foreach(token IN LISTS SUPPORTED_FILE_TYPES)
		if(token STREQUAL "EXT")
			set(in_ext TRUE)
			set(in_group FALSE)
		elseif(token STREQUAL "GROUP")
			set(in_ext FALSE)
			set(in_group TRUE)
		elseif(token IN_LIST _FT_ANY_TOKENS)
			set(in_ext FALSE)
			set(in_group FALSE)
		elseif(in_group)
			set(current_group ${token})
		elseif(in_ext AND current_group STREQUAL group)
			list(APPEND exts "*.${token}")
		endif()
	endforeach()
	list(JOIN exts " " exts)
	set(${out_var} "${exts}" PARENT_SCOPE)
endfunction()

#[[
Gets registered file extensions and URL protocols in a format suitable for
use in a WiX component.
#]]
function(get_wix_extensions prefix exe_name out_var)
	_wxs_generate_extensions(exts "${prefix}" "CM_FP_${exe_name}")
	_wxs_generate_urls(urls "${exe_name}")
	set(${out_var} "${exts}${urls}" PARENT_SCOPE)
endfunction()

#[[
Gets registered file extensions and URL protocols in a format suitable for
use in an XDG desktop file.
#]]
function(get_xdg_extensions out_var)
	_xdg_generate_extensions(exts)
	_xdg_generate_urls(urls)
	if(exts AND urls)
		set(exts "${exts};")
	endif()
	set(${out_var} "${exts}${urls}" PARENT_SCOPE)
endfunction()

#[[
Generates XDG mime-info files for the exported file types.
#]]
function(generate_xdg_mime_info out_dir)
	set(MIME "")
	set(export FALSE)
	# `ITEMS TYPE` is a hack to avoid duplicating the output logic by adding a
	# terminator token
	foreach(token IN LISTS SUPPORTED_FILE_TYPES ITEMS TYPE)
		if(token STREQUAL "TYPE")
			if(MIME AND export)
				list(JOIN NAME " " NAME)
				set(mime_info
					"<mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n"
					"\t<mime-type type=\"${MIME}\">\n"
					"\t\t<comment>${NAME}</comment>\n"
				)

				if(MAGIC)
					list(APPEND mime_info
						"\t\t<magic>\n"
						"\t\t\t<match type=\"string\" value=\"${MAGIC}\" offset=\"0\"/>\n"
						"\t\t</magic>\n"
					)
				endif()

				foreach(_ext IN LISTS EXT)
					list(APPEND mime_info
						"\t\t<glob pattern=\"*.${_ext}\"/>\n"
					)
				endforeach()

				list(APPEND mime_info
					"\t</mime-type>\n"
					"</mime-info>\n"
				)
				list(JOIN mime_info "" mime_info)
				file(WRITE "${out_dir}/${MIME}.xml" "${mime_info}")
			endif()

			foreach(var_name IN LISTS _FT_VALUE_TOKENS)
				set(${var_name} "")
			endforeach()
			set(in_var "")
			set(export FALSE)
		elseif(token STREQUAL "EXPORTED")
			set(export TRUE)
		elseif(token IN_LIST _FT_VALUE_TOKENS)
			set(in_var ${token})
		elseif(token IN_LIST _FT_FLAG_TOKENS)
			# ignore
		else()
			list(APPEND ${in_var} ${token})
		endif()
	endforeach()
endfunction()

# CMake is an abomination for content generation
function(_plist_generate_extensions out_var icon)
	set(dts "\t<key>CFBundleDocumentTypes</key>\n\t<array>\n")
	set(EXPORTED "")
	set(IMPORTED "")

	set(UTI "")
	# `ITEMS TYPE` is a hack to avoid duplicating the output logic by adding a
	# terminator token
	foreach(token IN LISTS SUPPORTED_FILE_TYPES ITEMS TYPE)
		if(token STREQUAL "TYPE")
			if(UTI)
				list(JOIN NAME " " NAME)
				list(APPEND dts
					"\t\t<dict>\n"
					"\t\t\t<key>CFBundleTypeRole</key>\n"
					"\t\t\t<string>Editor</string>\n"
					"\t\t\t<key>CFBundleTypeIconFile</key>\n"
					"\t\t\t<string>${icon}</string>\n"
					"\t\t\t<key>CFBundleTypeName</key>\n"
					"\t\t\t<string>${NAME}</string>\n"
					"\t\t\t<key>LSItemContentTypes</key>\n"
					"\t\t\t<array>\n"
					"\t\t\t\t<string>${UTI}</string>\n"
					"\t\t\t</array>\n"
					"\t\t</dict>\n"
				)

				if(type_decl)
					list(APPEND ${type_decl}
						"\t\t<dict>\n"
						"\t\t\t<key>UTTypeIconFile</key>\n"
						"\t\t\t<string>${icon}</string>\n"
						"\t\t\t<key>UTTypeDescription</key>\n"
						"\t\t\t<string>${NAME}</string>\n"
						"\t\t\t<key>UTTypeIdentifier</key>\n"
						"\t\t\t<string>${UTI}</string>\n"
						"\t\t\t<key>UTTypeTagSpecification</key>\n"
						"\t\t\t<dict>\n"
					)

					# start of UTTypeTagSpecification

					if(MIME)
						list(APPEND ${type_decl}
							"\t\t\t\t<key>public.mime-type</key>\n"
							"\t\t\t\t<string>${MIME}</string>\n"
						)
					endif()

					if(EXT)
						list(APPEND ${type_decl}
							"\t\t\t\t<key>public.filename-extension</key>\n"
							"\t\t\t\t<array>\n"
						)
						foreach(_ext IN LISTS EXT)
							list(APPEND ${type_decl}
								"\t\t\t\t\t<string>${_ext}</string>\n"
							)
						endforeach()
						list(APPEND ${type_decl} "\t\t\t\t</array>\n")
					endif()

					# end of UTTypeTagSpecification
					list(APPEND ${type_decl} "\t\t\t</dict>\n")

					if(CONFORMS_TO)
						list(APPEND ${type_decl}
							"\t\t\t<key>UTTypeConformsTo</key>\n"
							"\t\t\t<array>\n"
						)
						foreach(_uti IN LISTS CONFORMS_TO)
							list(APPEND ${type_decl}
								"\t\t\t\t<string>${_uti}</string>\n"
							)
						endforeach()
						list(APPEND ${type_decl}
							"\t\t\t</array>\n"
						)
					endif()

					if(REFERENCE)
						list(APPEND ${type_decl}
							"\t\t\t<key>UTTypeReferenceURL</key>\n"
							"\t\t\t<string>${REFERENCE}</string>\n"
						)
					endif()

					list(APPEND ${type_decl} "\t\t</dict>\n")
				endif()
			endif()

			foreach(var_name IN LISTS _FT_VALUE_TOKENS)
				set(${var_name} "")
			endforeach()
			set(type_decl "")
			set(in_var "")
		elseif(token IN_LIST _FT_VALUE_TOKENS)
			set(in_var ${token})
		elseif(token IN_LIST _FT_FLAG_TOKENS)
			set(type_decl ${token})
		else()
			list(APPEND ${in_var} ${token})
		endif()
	endforeach()

	list(APPEND dts "\t</array>\n")

	if(EXPORTED)
		list(APPEND dts
			"\t<key>UTExportedTypeDeclarations</key>\n"
			"\t<array>\n${EXPORTED}\n\t</array>\n"
		)
	endif()

	if(IMPORTED)
		list(APPEND dts
			"\t<key>UTImportedTypeDeclarations</key>\n"
			"\t<array>\n${IMPORTED}\n\t</array>\n"
		)
	endif()

	list(JOIN dts "" dts)
	set(${out_var} "${dts}" PARENT_SCOPE)
endfunction()

function(_plist_generate_urls out_var icon)
	set(dts "\t<key>CFBundleURLTypes</key>\n\t<array>\n")

	set(SCHEME "")
	# `ITEMS PROTOCOL` is a hack to avoid duplicating the output logic by adding
	# a terminator token
	foreach(token IN LISTS SUPPORTED_URL_PROTOCOLS ITEMS PROTOCOL)
		if(token STREQUAL "PROTOCOL")
			if(SCHEME)
				list(JOIN NAME " " NAME)
				list(APPEND dts
					"\t\t<dict>\n"
					"\t\t\t<key>CFBundleTypeRole</key>\n"
					"\t\t\t<string>Editor</string>\n"
					"\t\t\t<key>CFBundleURLIconFile</key>\n"
					"\t\t\t<string>${icon}</string>\n"
					"\t\t\t<key>CFBundleURLName</key>\n"
					"\t\t\t<string>${NAME}</string>\n"
					"\t\t\t<key>CFBundleURLSchemes</key>\n"
					"\t\t\t<array>\n"
					"\t\t\t\t<string>${SCHEME}</string>\n"
					"\t\t\t</array>\n"
					"\t\t</dict>\n"
				)
			endif()

			foreach(var_name IN LISTS _URL_VALUE_TOKENS)
				set(${var_name} "")
			endforeach()
			set(in_var "")
		elseif(token IN_LIST _URL_VALUE_TOKENS)
			# Data keyword
			set(in_var ${token})
		else()
			# Data
			list(APPEND ${in_var} ${token})
		endif()
	endforeach()

	list(JOIN dts "" dts)
	set(${out_var} "${dts}\t</array>\n" PARENT_SCOPE)
endfunction()

function(_wxs_generate_extensions out_var prefix target_id)
	set(dts "")

	set(UTI "")
	# `ITEMS TYPE` is a hack to avoid duplicating the output logic by adding a
	# terminator token
	foreach(token IN LISTS SUPPORTED_FILE_TYPES ITEMS TYPE)
		if(token STREQUAL "TYPE")
			if(UTI)
				list(JOIN NAME " " NAME)
				# Each extension needs its own ProgId for some reason or else
				# the generator fails with hash collisions. This is probably
				# wrong but the documentation gives no useful information.
				foreach(_ext IN LISTS EXT)
					list(APPEND dts
						"\t\t<ProgId Id=\"${prefix}.${UTI}.${_ext}\" Description=\"${NAME}\" Icon=\"${target_id}\">\n"
						"\t\t\t<Extension Id=\"${_ext}\" ContentType=\"${MIME}\">\n"
						"\t\t\t\t<Verb Id=\"open\" TargetFile=\"${target_id}\" Argument=\"&quot\;%1&quot\;\"/>\n"
						"\t\t\t</Extension>\n"
						"\t\t</ProgId>\n"
					)
				endforeach()
			endif()

			foreach(var_name IN LISTS _FT_VALUE_TOKENS)
				set(${var_name} "")
			endforeach()
			set(type_decl "")
			set(in_var "")
		elseif(token IN_LIST _FT_VALUE_TOKENS)
			set(in_var ${token})
		elseif(token IN_LIST _FT_FLAG_TOKENS)
			# Flag keyword
			set(type_decl ${token})
		else()
			list(APPEND ${in_var} ${token})
		endif()
	endforeach()

	list(JOIN dts "" dts)
	set(${out_var} "${dts}" PARENT_SCOPE)
endfunction()

function(_wxs_generate_urls out_var exe)
	set(dts "")

	set(SCHEME "")
	# `ITEMS PROTOCOL` is a hack to avoid duplicating the output logic by adding
	# a terminator token
	foreach(token IN LISTS SUPPORTED_URL_PROTOCOLS ITEMS PROTOCOL)
		if(token STREQUAL "PROTOCOL")
			if(SCHEME)
				list(JOIN NAME " " NAME)
				list(APPEND dts
					"\t\t<RegistryKey Key=\"${SCHEME}\" Root=\"HKCR\">\n"
					"\t\t\t<RegistryValue Name=\"URL Protocol\" Value=\"\" Type=\"string\"/>\n"
					"\t\t\t<RegistryValue Value=\"URL: ${NAME}\" Type=\"string\"/>\n"
					"\t\t\t<RegistryKey Key=\"DefaultIcon\">\n"
					"\t\t\t\t<RegistryValue Value=\"[INSTALLDIR]${exe}\" Type=\"string\"/>\n"
					"\t\t\t</RegistryKey>\n"
					"\t\t\t<RegistryKey Key=\"shell\\open\\command\">\n"
					"\t\t\t\t<RegistryValue Value=\"&quot\;[INSTALLDIR]${exe}&quot\; &quot\;%1&quot\;\" Type=\"string\"/>\n"
					"\t\t\t</RegistryKey>\n"
					"\t\t</RegistryKey>\n"
				)
			endif()

			foreach(var_name IN LISTS _URL_VALUE_TOKENS)
				set(${var_name} "")
			endforeach()
			set(in_var "")
		elseif(token IN_LIST _URL_VALUE_TOKENS)
			set(in_var ${token})
		else()
			list(APPEND ${in_var} ${token})
		endif()
	endforeach()

	list(JOIN dts "" dts)
	set(${out_var} "${dts}" PARENT_SCOPE)
endfunction()

function(_xdg_generate_extensions out_var)
	set(mimes "")
	set(in_var FALSE)
	foreach(token IN LISTS SUPPORTED_FILE_TYPES)
		if(token STREQUAL "MIME")
			set(in_var TRUE)
		elseif(token IN_LIST _FT_ANY_TOKENS)
			set(in_var FALSE)
		elseif(in_var)
			list(APPEND mimes ${token})
		endif()
	endforeach()
	set(${out_var} ${mimes} PARENT_SCOPE)
endfunction()

function(_xdg_generate_urls out_var)
	set(mimes "")
	foreach(token IN LISTS SUPPORTED_URL_PROTOCOLS)
		if(token STREQUAL "SCHEME")
			set(in_var TRUE)
		elseif(token IN_LIST _URL_ANY_TOKENS)
			set(in_var FALSE)
		elseif(in_var)
			list(APPEND mimes "x-scheme-handler/${token}")
		endif()
	endforeach()

	set(${out_var} "${mimes}" PARENT_SCOPE)
endfunction()
