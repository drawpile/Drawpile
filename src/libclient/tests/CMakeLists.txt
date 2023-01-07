find_package(${QT_PACKAGE_NAME} ${DP_MIN_QT_VERSION_GUI} REQUIRED COMPONENTS Test)

add_unit_tests(client
	SOURCES resources.qrc
	LIBS dpclient ${QT_PACKAGE_NAME}::Test
	TESTS html listingfiltering newversion
)
