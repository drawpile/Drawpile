function(_get_shared_library_path out_var target)
	get_target_property(type ${target} TYPE)
	if(NOT type STREQUAL "SHARED_LIBRARY" AND NOT type STREQUAL "UNKNOWN_LIBRARY")
		return()
	endif()

	get_target_property(location ${target} LOCATION)

	# For unknown libraries on Win32, the location only contains the .lib, so we
	# must hunt down the actual DLL.
	if(WIN32 AND type STREQUAL "UNKNOWN_LIBRARY")
		find_program(librarian lib REQUIRED)
		execute_process(
			COMMAND "${librarian}" /NOLOGO /LIST "${location}"
			OUTPUT_VARIABLE ar_content
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		# If the .lib file has .obj references then it is actually a static
		# library. One would hope there is a better way to do this but after
		# a day of looking, this seems like the easiest path. Note that it is
		# possible for a .lib file to have both .obj and .dll entries
		# (libmicrohttpd does this), so it is not safe to just look for a .dll.
		if(ar_content MATCHES "\.[Oo][Bb][Jj][\r\n]")
			message(DEBUG "${target} looks like a static library")
			return()
		elseif(ar_content MATCHES "\n([^\r\n]*\.[Dd][Ll][Ll])[\r\n]")
			set(dll_name "${CMAKE_MATCH_1}")
		else()
			return()
		endif()

		list(TRANSFORM CMAKE_PREFIX_PATH APPEND "/bin" OUTPUT_VARIABLE hints)
		get_filename_component(lib_dir "${location}" DIRECTORY)
		get_filename_component(base_name "${location}" NAME_WE)
		# CMake may have picked up something within a subdirectory of the lib
		# directory (this happens for OpenSSL) so it is not possible to just
		# strip the filename
		string(FIND "${location}" "/lib/" lib_pos REVERSE)
		string(SUBSTRING "${location}" 0 ${lib_pos} prefix)

		unset(location)
		find_file(location "${dll_name}" NO_CACHE REQUIRED
			NO_DEFAULT_PATH
			HINTS
				${prefix}/bin
				${hints}
			PATHS
				ENV PATH
		)
	endif()

	if(NOT location)
		message(DEBUG "Missing ${target} location")
	endif()

	# Qt really, really wants us to add the custom suffixed OpenSSL library for
	# Android, not a bare libssl.so or versioned symlink, even though everything
	# else only works because the symlink exists
	while(IS_SYMLINK "${location}")
		file(READ_SYMLINK "${location}" dest)
		if(NOT IS_ABSOLUTE "${dest}")
			get_filename_component(dir "${location}" DIRECTORY)
			set(location "${dir}/${dest}")
		else()
			set(location "${dest}")
		endif()
	endwhile()

	# Anything in sysroot is a system dependency that should not be bundled
	string(FIND "${location}" "${CMAKE_SYSROOT}" sysroot_pos)
	if(CMAKE_SYSROOT AND sysroot_pos EQUAL 0)
		message(DEBUG "Skipping ${target} because in sysroot")
		return()
	endif()

	message(DEBUG "${target} found at '${location}'")
	set(${out_var} "${location}" PARENT_SCOPE)
endfunction()

function(_get_non_qt_shared_libs out_var)
	foreach(target IN LISTS ARGN)
		# Matching `^-` to ignore junk like `-pthreads`
		if(NOT target OR target MATCHES "^-")
			message(DEBUG "Skipping ${target}")
			continue()
		endif()

		# Some targets may not be visible from this directory if they
		# were found in a sibling directory, so try to find them as
		# packages so they become visible here too
		if(NOT TARGET ${target})
			string(FIND ${target} "::" sep)
			if(sep EQUAL -1)
				find_package(${target} QUIET)
			else()
				string(SUBSTRING "${target}" 0 ${sep} package)
				math(EXPR sep "${sep} + 2")
				string(SUBSTRING "${target}" ${sep} -1 component)
				# At least libzip is broken and will fail if COMPONENTS
				# is given, so try with no COMPONENTS first and then if
				# the component target still does not exist, try again
				# with more COMPONENTS
				find_package(${package} QUIET)
				if(NOT TARGET ${target})
					find_package(${package} COMPONENTS ${component} QUIET)
				endif()
			endif()
		endif()

		# Do not recurse through any Qt targets because the Qt deployment
		# tools are at least smart enough to always pick those correctly
		if(TARGET ${target} AND NOT target MATCHES "^${QT_PACKAGE_NAME}::")
			_get_shared_library_path(lib ${target})
			if(lib)
				list(APPEND libs ${lib})
			endif()
			get_target_property(links ${target} LINK_LIBRARIES)
			get_target_property(iface ${target} INTERFACE_LINK_LIBRARIES)
			list(APPEND links ${iface})
			string(GENEX_STRIP "${links}" links)
			foreach(link IN LISTS links)
				_get_non_qt_shared_libs(link_libs ${link})
				list(APPEND libs ${link_libs})
			endforeach()
		endif()
	endforeach()

	list(REMOVE_DUPLICATES libs)
	set(${out_var} ${libs} PARENT_SCOPE)
endfunction()

#[[
Gets all of the non-Qt shared libraries that must be distributed for the given
target to run.
#]]
function(get_shared_libs out_var target)
	if(ANDROID OR (WIN32 AND QT_VERSION_MAJOR EQUAL 5))
		# Qt dlopens OpenSSL libraries so they must be included too
		set(extras OpenSSL::Crypto OpenSSL::SSL)
	endif()

	_get_non_qt_shared_libs(libs ${target} ${extras})
	message(STATUS "Shared libraries for ${target}: ${libs}")
	set(${out_var} ${libs} PARENT_SCOPE)
endfunction()
