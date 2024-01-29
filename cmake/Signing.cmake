# SPDX-License-Identifier: MIT

if(WIN32 AND DEFINED ENV{WINDOWS_PFX_PATH})
    set(HAVE_CODE_SIGNING ON)
    find_program(SIGNTOOL_COMMAND signtool REQUIRED)
    function(dp_sign_executable target)
        message(STATUS "Will sign executable for ${target}")
		add_custom_command(TARGET "${target}" POST_BUILD
			COMMAND
                ${SIGNTOOL_COMMAND} "sign" "/v"
                "/f" "\"%WINDOWS_PFX_PATH%\""
                "/p" "\"%WINDOWS_PFX_PASS%\""
                "/t" "\"%WINDOWS_PFX_TIMESTAMP_URL%\""
                "/fd" "SHA256"
                "$<TARGET_FILE:${target}>"
			COMMAND_EXPAND_LISTS
			COMMENT "Signing executable for ${target}"
		)
    endfunction()
else()
    set(HAVE_CODE_SIGNING OFF)
    function(dp_sign_executable target)
        message(DEBUG "Code signing not available, not signing ${target}")
    endfunction()
endif()

add_feature_info("Sign executables" HAVE_CODE_SIGNING "")
