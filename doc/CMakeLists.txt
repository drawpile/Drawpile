install(
	FILES
		../ChangeLog
		../COPYING
		../README.md
		openraster.txt
		textloader.md
		protocol.md
		messages.md
		logins.md
	DESTINATION "share/doc/drawpile"
)
install(
	FILES
		drawpile-srv.1
	DESTINATION "share/man/man1"
)
