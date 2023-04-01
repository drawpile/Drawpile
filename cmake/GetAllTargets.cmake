#[[
Gets the list of all targets in the given directory.
#]]
function(get_all_targets out_var current_dir)
	get_property(targets DIRECTORY ${current_dir} PROPERTY BUILDSYSTEM_TARGETS)
	get_property(subdirs DIRECTORY ${current_dir} PROPERTY SUBDIRECTORIES)

	foreach(subdir ${subdirs})
		get_all_targets(subdir_targets ${subdir})
		list(APPEND targets ${subdir_targets})
	endforeach()

	set(${out_var} ${targets} PARENT_SCOPE)
endfunction()
