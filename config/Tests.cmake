macro(AddUnitTest test)
	add_executable("test_${TEST_PREFIX}_${test}" "${test}.cpp" ${TestResources})
	set_drawpile_target_properties("test_${TEST_PREFIX}_${test}")
	target_link_libraries("test_${TEST_PREFIX}_${test}" ${TEST_LIBS})

	add_test(
		NAME "${TEST_PREFIX}_${test}"
		COMMAND "test_${TEST_PREFIX}_${test}")

	# Use the offscreen platform module so tests can run headlessly
	set_property(
		TEST "${TEST_PREFIX}_${test}" APPEND PROPERTY
		ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endmacro()

