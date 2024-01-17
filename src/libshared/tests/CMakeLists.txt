find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Test)

set(dpshared_tests passwordhash filename listings ulid)
if(HAVE_TCPSOCKETS)
	list(APPEND dpshared_tests messagequeue)
endif()
add_unit_tests(shared
	LIBS dpshared ${QT_PACKAGE_NAME}::Test
	TESTS ${dpshared_tests}
)

if(TARGET libsodium::libsodium)
	add_unit_test(shared authtoken LIBS dpshared ${QT_PACKAGE_NAME}::Test)
endif()
