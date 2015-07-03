find_program(XDGICON xdg-icon-resource)
find_program(XDGMENU xdg-desktop-menu)
find_program(XDGMIME xdg-mime)

if( XDGICON )
	install(CODE "
		execute_process(COMMAND ${XDGICON} install --novendor ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-16x16.png --size 16 drawpile)
		execute_process(COMMAND ${XDGICON} install --novendor ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-32x32.png --size 32 drawpile)
		execute_process(COMMAND ${XDGICON} install --novendor ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-64x64.png --size 64 drawpile)
		execute_process(COMMAND ${XDGICON} install --novendor ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-128x128.png --size 128 drawpile)
		execute_process(COMMAND ${XDGICON} install --novendor ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-256x256.png --size 256 drawpile)

		execute_process(COMMAND ${XDGICON} install --context mimetypes ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-16x16.png --size 16 application-x-drawpile-recording)
		execute_process(COMMAND ${XDGICON} install --context mimetypes ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-32x32.png --size 32 application-x-drawpile-recording)
		execute_process(COMMAND ${XDGICON} install --context mimetypes ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-64x64.png --size 64 application-x-drawpile-recording)
		execute_process(COMMAND ${XDGICON} install --context mimetypes ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-128x128.png --size 128 application-x-drawpile-recording)
		execute_process(COMMAND ${XDGICON} install --context mimetypes ${CMAKE_CURRENT_SOURCE_DIR}/drawpile-256x256.png --size 256 application-x-drawpile-recording)
	")
endif()

if( XDGMENU )
	install(CODE "
		execute_process(COMMAND ${XDGMENU} install --novendor ${CMAKE_CURRENT_SOURCE_DIR}/drawpile.desktop)
	")
endif()

if( XDGMIME )
	install(CODE "
		execute_process(COMMAND ${XDGMIME} install ${CMAKE_CURRENT_SOURCE_DIR}/x-drawpile.xml)
	")
endif()

set(DATADIR "${CMAKE_INSTALL_PREFIX}/share/drawpile/drawpile")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/palettes" DESTINATION "${DATADIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sounds" DESTINATION "${DATADIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/theme" DESTINATION "${DATADIR}")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/drawpile.appdata.xml" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/appdata/")

