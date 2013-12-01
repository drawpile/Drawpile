SET(base_files
	../ChangeLog
	../COPYING
	../README.md
	openraster.txt
	textloader.txt
)

SET(man1
	drawpile-srv.1
)

if (WIN32)
	install (
		FILES ${base_files}
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
