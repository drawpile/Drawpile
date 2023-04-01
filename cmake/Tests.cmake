#[[
Adds unit test executables.
#]]
function(add_unit_tests prefix)
	set(multiValueArgs LIBS SOURCES TESTS DEFINITIONS)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "${multiValueArgs}")

	if(NOT prefix)
		message(FATAL_ERROR "missing required prefix")
		return()
	endif()

	foreach(name IN LISTS ARG_TESTS)
		if(name)
			add_unit_test("${prefix}" "${name}" LIBS ${ARG_LIBS} SOURCES ${ARG_SOURCES})
		endif()
	endforeach()
endfunction()

#[[
Adds a single unit test executable.
#]]
function(add_unit_test prefix name)
	set(multiValueArgs LIBS SOURCES DEFINITIONS)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "${multiValueArgs}")

	if(NOT prefix)
		message(FATAL_ERROR "missing required prefix")
		return()
	endif()

	if(NOT name)
		message(FATAL_ERROR "missing required name")
		return()
	endif()

	set(test_name "${prefix}_${name}")
	set(target_name "test_${test_name}")

	add_executable(${target_name}
		"${name}.cpp"
		${ARG_SOURCES}
	)
	target_link_libraries(${target_name} ${ARG_LIBS})
	if(ARG_DEFINITIONS)
		target_compile_definitions(${target_name} PRIVATE ${ARG_DEFINITIONS})
	endif()

	add_test(
		NAME ${test_name}
		COMMAND ${target_name}
	)

	# Use the offscreen platform module so tests can run headlessly
	set_property(
		TEST ${test_name}
		APPEND PROPERTY
		ENVIRONMENT QT_QPA_PLATFORM=offscreen
	)
endfunction()
