add_custom_target (
	update
	COMMAND svn update
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT Update local source tree to latest revision
)
