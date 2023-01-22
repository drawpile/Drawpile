find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Test)

add_unit_tests(shared
	LIBS dpshared ${QT_PACKAGE_NAME}::Test
	TESTS passwordhash filename messages recording messagequeue listings ulid
)

if(Sodium_FOUND)
	add_unit_test(shared authtoken LIBS dpshared ${QT_PACKAGE_NAME}::Test)
endif()
