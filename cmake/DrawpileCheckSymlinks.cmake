if(WIN32)
	file(TOUCH "${CMAKE_CURRENT_BINARY_DIR}/check_symlinks")
	file(CREATE_LINK
		"${CMAKE_CURRENT_BINARY_DIR}/check_symlinks"
		"${CMAKE_CURRENT_BINARY_DIR}/check_symlinks_ok"
		RESULT no_symlinks
		SYMBOLIC
	)
	file(REMOVE
		"${CMAKE_CURRENT_BINARY_DIR}/check_symlinks"
		"${CMAKE_CURRENT_BINARY_DIR}/check_symlinks_ok"
	)
	if(no_symlinks)
		message(FATAL_ERROR "Building Drawpile requires support for symlinks. "
			"Go to the \"For developers\" settings page in Windows and enable "
			"Developer Mode, or (not recommended) run CMake from an elevated "
			"command prompt. See "
			"<https://docs.microsoft.com/en-us/windows/uwp/get-started/enable-your-device-for-development> for more details."
		)
	endif()
	unset(no_symlinks)
endif()
