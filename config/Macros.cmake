if ( WIN32 )
	find_file ( STRIP_CMD strip.exe )
else ( WIN32 )
	find_file ( STRIP_CMD strip )
endif ( WIN32 )

macro ( strip_exe target )
	if ( STRIP_CMD )
		if ( WIN32 )
			set ( target_file "${target}.exe" )
		else ( WIN32 )
			set ( target_file "${target}" )
		endif ( WIN32 )
		
		add_custom_command(
			TARGET ${target}
			POST_BUILD
			COMMAND ${STRIP_CMD} -s "${EXECUTABLE_OUTPUT_PATH}/${target_file}"
		)
	endif ( STRIP_CMD )
endmacro ( strip_exe target )

macro ( generate_win32_resource resfile FULLNAME INTERNALNAME DESCRIPTION COMMENT COPYRIGHT VERSION_MAJOR VERSION_MINOR VERSION_BUG )
	if ( WIN32 )
		set ( win32RC ${CMAKE_CURRENT_BINARY_DIR}/win32resource.rc )
		if ( ${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN ${win32RC} )
			file ( WRITE ${win32RC} "#include <winver.h>\n\n" )
			file ( APPEND ${win32RC} "VS_VERSION_INFO VERSIONINFO\n" )
			file ( APPEND ${win32RC} "\tFILEVERSION ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_BUG},0\n" )
			file ( APPEND ${win32RC} "\tPRODUCTVERSION ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_BUG},0\n" )
			file ( APPEND ${win32RC} "\tFILEFLAGSMASK VS_FFI_FILEFLAGSMASK\n" )
			file ( APPEND ${win32RC} "\t#ifdef _DEBUG\n\tFILEFLAGS VS_FF_DEBUG\n\t#else\n\tFILEFLAGS 0\n\t#endif\n" )
			file ( APPEND ${win32RC} "\tFILEOS VOS_NT\n" )
			if ( IsSharedLib )
				file ( APPEND ${win32RC} "\tFILETYPE VFT_DLL\n" )
			else ( IsSharedLib )
				file ( APPEND ${win32RC} "\tFILETYPE VFT_APP\n" )
			endif ( IsSharedLib )
			file ( APPEND ${win32RC} "\tFILESUBTYPE 0\n" )
			file ( APPEND ${win32RC} "BEGIN\n\tBLOCK \"StringFileInfo\"\n\tBEGIN\n" )
			file ( APPEND ${win32RC} "\t\tBLOCK \"040904E4\"\n\t\tBEGIN\n" )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"Comments\", \"${COMMENT}\\0\"\n" )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"FileDescription\", \"${DESCRIPTION}\\0\"\n" )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"FileVersion\", \"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUG}\\0\"\n" )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"InternalName\", \"${INTERNALNAME}\\0\"\n" )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"LegalCopyright\", \"Copyright (c) ${COPYRIGHT}\\0\"\n" )
			if ( IsSharedLib )
				file ( APPEND ${win32RC} "\t\t\tVALUE \"OriginalFilename\", \"${INTERNALNAME}.dll\\0\"\n" )
			else ( IsSharedLib )
				file ( APPEND ${win32RC} "\t\t\tVALUE \"OriginalFilename\", \"${INTERNALNAME}.exe\\0\"\n" )
			endif ( IsSharedLib )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"ProductName\", \"${FULLNAME}\\0\"\n" )
			file ( APPEND ${win32RC} "\t\t\tVALUE \"ProductVersion\", \"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUG}\\0\"\n" )
			file ( APPEND ${win32RC} "\t\tEND\n\tEND\n" )
			file ( APPEND ${win32RC} "\tBLOCK \"VarFileInfo\"\n\tBEGIN\n" )
			file ( APPEND ${win32RC} "\t\tVALUE \"Translation\", 0x409, 1252\n" )
			file ( APPEND ${win32RC} "\tEND\nEND\n" )
		endif ( ${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN ${win32RC} )
		
		set ( ${resfile} ${CMAKE_CURRENT_BINARY_DIR}/win32resource.obj )
		
		add_custom_command(
			OUTPUT ${resfile}
			COMMAND windres ${win32RC} ${resfile}
			DEPENDS ${win32RC}
		)
	endif ( WIN32 )
endmacro ( generate_win32_resource )

set ( __GENFINALDIR "" )
set ( __GENFINALNUM 1 )

macro ( generate_final OUTFILE )
	# allow multiple final files per directory
	if ( __GENFINALDIR STREQUAL CMAKE_CURRENT_BINARY_DIR )
		MATH ( EXPR __GENFINALNUM "${__GENFINALNUM} + 1" )
	else ( __GENFINALDIR STREQUAL CMAKE_CURRENT_BINARY_DIR )
		set ( __GENFINALNUM 1 )
	endif ( __GENFINALDIR STREQUAL CMAKE_CURRENT_BINARY_DIR )
	
	set ( finalfile ${CMAKE_CURRENT_BINARY_DIR}/_final${__GENFINALNUM}.cpp )
	set ( __GENFINALDIR ${CMAKE_CURRENT_BINARY_DIR} )
	
	file ( WRITE ${finalfile} "// Automatically generated\n\n" )
	foreach (it ${ARGN})
		file ( APPEND "${finalfile}" "#include \"${it}\"\n" )
		MACRO_ADD_FILE_DEPENDENCIES( ${${OUTFILE}} ${it} )
	endforeach ( it )
	
	set ( ${OUTFILE} ${finalfile} )
endmacro ( generate_final OUTFILE )

macro ( InSourceDir nFiles )
	set ( filelist )
	foreach (it ${ARGN})
		list( APPEND filelist ${CMAKE_CURRENT_SOURCE_DIR}/${it} )
		MACRO_ADD_FILE_DEPENDENCIES( ${${nFiles}} ${CMAKE_CURRENT_SOURCE_DIR}/${it} )
	endforeach ( it )
	set ( ${nFiles} ${filelist} )
endmacro ( InSourceDir nFiles )

macro ( InBuildDir nFiles )
	set ( filelist )
	foreach (it ${ARGN})
		list ( APPEND filelist ${CMAKE_CURRENT_BINARY_DIR}/${it} )
		MACRO_ADD_FILE_DEPENDENCIES( ${${nFiles}} ${CMAKE_CURRENT_BINARY_DIR}/${it} )
	endforeach ( it )
	set ( ${nFiles} ${filelist} )
endmacro ( InBuildDir files )
