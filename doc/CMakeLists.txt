SET(base_files drawpile.docbook
	../ChangeLog
	../COPYING
	)

SET(man1 drawpile-srv.1)

INSTALL(FILES ${base_files}
        DESTINATION "share/doc/drawpile")

INSTALL(FILES ${man1}
        DESTINATION "share/man/man1")

