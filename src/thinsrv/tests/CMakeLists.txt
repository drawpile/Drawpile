find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Network Sql Test)
find_package(${QT_PACKAGE_NAME}${Qt_VERSION_MAJOR} ${DP_MIN_QT_VERSION_SERVER} REQUIRED COMPONENTS Network Sql Test)

add_unit_tests(thinsrv
	SOURCES resources.qrc
	LIBS ${srvlib} ${QT_PACKAGE_NAME}::Network ${QT_PACKAGE_NAME}::Sql ${QT_PACKAGE_NAME}::Test
	TESTS serverconfig templates dblog
)
