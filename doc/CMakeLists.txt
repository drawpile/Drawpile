SET(base_files
	../ChangeLog
	../COPYING
	../README.md
	openraster.txt
	textloader.md
	protocol.md
	messages.md
	logins.md
)

SET(man1
	drawpile-srv.1
)

if (UNIX AND NOT APPLE)
	install (
		FILES ${base_files}
		DESTINATION "share/doc/drawpile"
		)
	INSTALL (
		FILES ${man1}
		DESTINATION "share/man/man1"
		)
endif ()
