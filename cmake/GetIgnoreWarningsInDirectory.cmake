# SPDX-License-Identifier: MIT

function(get_ignore_warnings_in_directory out_var)
	get_directory_property(options COMPILE_OPTIONS)
	if(MSVC)
		list(TRANSFORM options REPLACE "/W[0-9]$" "/W0")
		list(TRANSFORM options REPLACE "/W([0-9]{4})" "/wd\\1")
	else()
		list(TRANSFORM options REPLACE "-W(no-)?([^=]+)(=.*$)?" "-Wno-\\2")
	endif()
	set(${out_var} "${options}" PARENT_SCOPE)
endfunction()
