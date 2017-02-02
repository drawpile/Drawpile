macro ( AddUnitTest test )
	add_executable("test_${TEST_PREFIX}_${test}" "${test}.cpp" ${TestResources})
	target_link_libraries("test_${TEST_PREFIX}_${test}" ${TEST_LIBS})
	add_test(
			NAME "${TEST_PREFIX}_${test}"
			COMMAND "test_${TEST_PREFIX}_${test}"
			)
endmacro()

