if ( WIN32 )
	find_file ( STRIP_CMD strip.exe )
else ( WIN32 )
	find_file ( STRIP_CMD strip )
endif ( WIN32 )

macro ( strip_exe target )
	if ( STRIP_CMD )
		add_custom_command(
			TARGET ${target}
			POST_BUILD
			COMMAND ${STRIP_CMD} ${target}
		)
	endif ( STRIP_CMD )
endmacro ( strip_exe target )
