if(WIN32)
	find_file(STRIP_CMD strip.exe)
else()
	find_file(STRIP_CMD strip)
endif()

macro(strip_exe target)
	if(STRIP_CMD)
		if(WIN32)
			set(target_file "${target}.exe")
		elseif(APPLE)
			set(target_file "${target}.app/Contents/MacOS/${target}")
		else()
			set(target_file "${target}")
		endif()

		add_custom_command(
			TARGET ${target}
			POST_BUILD
			COMMAND ${STRIP_CMD} "${EXECUTABLE_OUTPUT_PATH}/${target_file}")
	endif()
endmacro()

macro(strip_lib target)
	if(STRIP_CMD)
		if(WIN32)
			set(target_file "${target}.dll")
		else()
			set(target_file "lib${target}")
		endif()

		add_custom_command(
			TARGET ${target}
			POST_BUILD
			COMMAND ${STRIP_CMD} -s "${LIBRARY_OUTPUT_PATH}/${target_file}")
	endif()
endmacro()

# "0.7.0" -> 0 7 0
function(split_version version major minor bug)
    # Transform the string in a list
    string(REPLACE "." ";" version_list ${version})
    # Extract the elements
    list(GET version_list 0 ma)
    list(GET version_list 1 mi)
    list(GET version_list 2 bu)
    # Assign the function parameters
    set(${major} ${ma} PARENT_SCOPE)
    set(${minor} ${mi} PARENT_SCOPE)
    set(${bug}   ${bu} PARENT_SCOPE)
endfunction()

macro(generate_win32_resource resfile FULLNAME INTERNALNAME DESCRIPTION COMMENT COPYRIGHT VERSION ICONFILE)
        split_version(${VERSION} VERSION_MAJOR VERSION_MINOR VERSION_BUG)
        message("+++ ${VERSION_MAJOR} ${VERSION_MINOR} ${VERSION_BUG}")
	if(WIN32)
		set(win32RC ${CMAKE_CURRENT_BINARY_DIR}/win32resource.rc)
		if(${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN ${win32RC})
			file(WRITE ${win32RC} "#include <winver.h>\n\n")
			file(APPEND ${win32RC} "IDI_ICON1 ICON DISCARDABLE \"${ICONFILE}\"\n\n")
			file(APPEND ${win32RC} "VS_VERSION_INFO VERSIONINFO\n")
			file(APPEND ${win32RC} "\tFILEVERSION ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_BUG},0\n")
			file(APPEND ${win32RC} "\tPRODUCTVERSION ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_BUG},0\n")
			file(APPEND ${win32RC} "\tFILEFLAGSMASK VS_FFI_FILEFLAGSMASK\n")
			file(APPEND ${win32RC} "\t#ifdef _DEBUG\n\tFILEFLAGS VS_FF_DEBUG\n\t#else\n\tFILEFLAGS 0\n\t#endif\n")
			file(APPEND ${win32RC} "\tFILEOS VOS_NT\n")
			if(IsSharedLib)
				file(APPEND ${win32RC} "\tFILETYPE VFT_DLL\n")
			else()
				file(APPEND ${win32RC} "\tFILETYPE VFT_APP\n")
			endif()
			file(APPEND ${win32RC} "\tFILESUBTYPE 0\n")
			file(APPEND ${win32RC} "BEGIN\n\tBLOCK \"StringFileInfo\"\n\tBEGIN\n")
			file(APPEND ${win32RC} "\t\tBLOCK \"040904E4\"\n\t\tBEGIN\n")
			file(APPEND ${win32RC} "\t\t\tVALUE \"Comments\", \"${COMMENT}\\0\"\n")
			file(APPEND ${win32RC} "\t\t\tVALUE \"FileDescription\", \"${DESCRIPTION}\\0\"\n")
			file(APPEND ${win32RC} "\t\t\tVALUE \"FileVersion\", \"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUG}\\0\"\n")
			file(APPEND ${win32RC} "\t\t\tVALUE \"InternalName\", \"${INTERNALNAME}\\0\"\n")
			file(APPEND ${win32RC} "\t\t\tVALUE \"LegalCopyright\", \"Copyright(c)${COPYRIGHT}\\0\"\n")
			if(IsSharedLib)
				file(APPEND ${win32RC} "\t\t\tVALUE \"OriginalFilename\", \"${INTERNALNAME}.dll\\0\"\n")
			else(IsSharedLib)
				file(APPEND ${win32RC} "\t\t\tVALUE \"OriginalFilename\", \"${INTERNALNAME}.exe\\0\"\n")
			endif(IsSharedLib)
			file(APPEND ${win32RC} "\t\t\tVALUE \"ProductName\", \"${FULLNAME}\\0\"\n")
			file(APPEND ${win32RC} "\t\t\tVALUE \"ProductVersion\", \"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUG}\\0\"\n")
			file(APPEND ${win32RC} "\t\tEND\n\tEND\n")
			file(APPEND ${win32RC} "\tBLOCK \"VarFileInfo\"\n\tBEGIN\n")
			file(APPEND ${win32RC} "\t\tVALUE \"Translation\", 0x409, 1252\n")
			file(APPEND ${win32RC} "\tEND\nEND\n")
		endif(${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN ${win32RC})

		set(${resfile} "${CMAKE_CURRENT_BINARY_DIR}/win32resource.obj")

		add_custom_command(
			OUTPUT ${${resfile}}
			COMMAND ${CMAKE_RC_COMPILER} ${win32RC} ${${resfile}}
			DEPENDS ${win32RC})
	endif()
endmacro()

macro(TestCompilerFlag FLAG FLAGLIST)
	check_cxx_accepts_flag(${${FLAG}} ACCEPTS_${FLAG}_FLAG)
	if(ACCEPTS_${FLAG}_FLAG)
		list(APPEND ${FLAGLIST} ${${FLAG}})
	endif()
endmacro()

macro(BundleQtPlugin PLUGIN PLUGINPATH)
	get_target_property(_loc "${PLUGIN}" LOCATION)
	install(FILES "${_loc}" DESTINATION "${CMAKE_INSTALL_PREFIX}/${CLIENTNAME}.app/Contents/PlugIns/${PLUGINPATH}")
endmacro()
