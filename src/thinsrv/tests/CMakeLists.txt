find_package(Qt5Test REQUIRED)

set(TEST_PREFIX server)

qt5_add_resources( TestResources resources.qrc )

set(
	TEST_LIBS
	"${SRVNAME}lib"
	Qt5::Test
	)

AddUnitTest(serverconfig)
AddUnitTest(templates)
AddUnitTest(dblog)

