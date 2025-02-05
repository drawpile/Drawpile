#[[
Sets the macOS deployment target to the minimum version supported by the given
Qt version.
#]]
function(set_mac_deployment_target qt_version)
	if(qt_version VERSION_GREATER_EQUAL 6.8)
		set(target 12.0)
	elseif(qt_version VERSION_GREATER_EQUAL 6.5)
		set(target 11.0)
	elseif(qt_version VERSION_GREATER_EQUAL 6.0)
		set(target 10.14)
	elseif(qt_version VERSION_GREATER_EQUAL 5.14)
		set(target 10.13)
	elseif(qt_version VERSION_GREATER_EQUAL 5.12)
		set(target 10.12)
	elseif(qt_version VERSION_GREATER_EQUAL 5.11)
		set(target 10.11)
	else()
		message(FATAL_ERROR "Qt version ${qt_version} too old")
	endif()
	set(CMAKE_OSX_DEPLOYMENT_TARGET ${target} CACHE STRING "macOS deployment target" FORCE)
endfunction()
