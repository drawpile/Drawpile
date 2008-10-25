SET(base_files drawpile.docbook
	../ChangeLog
	../COPYING
	)

SET(man1 drawpile-srv.1)

if (WIN32)
	install (
		FILES ${base_files}
		DESTINATION .
		)
	INSTALL (
		FILES ${man1}
		DESTINATION .
		)	
else (WIN32)
	install (
		FILES ${base_files}
		DESTINATION "share/doc/drawpile"
		)
	INSTALL (
		FILES ${man1}
		DESTINATION "share/man/man1"
		)
endif (WIN32)
