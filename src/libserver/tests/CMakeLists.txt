find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Test)

add_unit_tests(server
	LIBS dpserver ${QT_PACKAGE_NAME}::Test
	TESTS filedhistory sessionban idqueue serverlog
)
