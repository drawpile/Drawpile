set(ICONDIR ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor)

install( FILES drawpile-16x16.png DESTINATION ${ICONDIR}/16x16/apps RENAME drawpile.png )
install( FILES drawpile-32x32.png DESTINATION ${ICONDIR}/32x32/apps RENAME drawpile.png )
install( FILES drawpile-64x64.png DESTINATION ${ICONDIR}/64x64/apps RENAME drawpile.png )
install( FILES drawpile-128x128.png DESTINATION ${ICONDIR}/128x128/apps RENAME drawpile.png )
install( FILES drawpile-256x256.png DESTINATION ${ICONDIR}/256x256/apps RENAME drawpile.png )
install( FILES drawpile.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications )

